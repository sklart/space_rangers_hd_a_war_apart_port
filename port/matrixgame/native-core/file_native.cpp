// Native backend for the original MatrixLib CFile API. GPL-2.0-or-later.
#include "CFile.hpp"
#include "CException.hpp"
#include "package.hpp"
#include <algorithm>
#include <climits>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <locale>
#include <limits>
#include <mutex>
#include <string_view>
#include <sys/stat.h>
#include <vector>

namespace Base {
namespace {
namespace fs = std::filesystem;
using Package = srhd_awa::package::Package;
struct PackageSlot { std::wstring path; std::shared_ptr<Package> package; };
struct Registry { std::mutex mutex; std::vector<PackageSlot> slots; size_t active = 0; };
Registry& Packages() { static Registry registry; return registry; }
std::string Utf8(std::wstring_view text) {
    if (text.find(L'\0') != text.npos) ERROR_S(L"NUL in file name");
    if constexpr (sizeof(wchar_t) == 2)
        return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(text.data(), text.data() + text.size());
    else
        return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(text.data(), text.data() + text.size());
}
std::wstring Wide(std::string_view text) {
    if constexpr (sizeof(wchar_t) == 2)
        return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(text.data(), text.data() + text.size());
    else
        return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(text.data(), text.data() + text.size());
}
std::wstring Normalize(std::wstring text) {
    std::replace(text.begin(), text.end(), L'\\', L'/');
    while (text.starts_with(L"./")) text.erase(0, 2);
    return text;
}
fs::path Path(std::wstring_view text) {
#ifdef _WIN32
    if (text.find(L'\0') != text.npos) ERROR_S(L"NUL in file name");
    return fs::path(text);
#else
    return fs::path(Utf8(text));
#endif
}
std::wstring PathWide(const fs::path& path) {
#ifdef _WIN32
    return path.wstring();
#else
    return Wide(path.native());
#endif
}
fs::path ResolveCase(const fs::path& path) {
    std::error_code error;
    if (fs::exists(path, error)) return path;
    fs::path current = path.root_path();
    for (const auto& component : path.relative_path()) {
        const auto candidate = current / component;
        if (fs::exists(candidate, error)) { current = candidate; continue; }
        const auto wanted = PathWide(component);
        bool found = false;
        for (const auto& entry : fs::directory_iterator(current.empty() ? fs::path(".") : current, error)) {
            auto actual = PathWide(entry.path().filename());
            if (actual.size() != wanted.size()) continue;
            bool same = true;
            for (size_t i = 0; i < actual.size(); ++i) {
                const auto lower = [](wchar_t c) { return c >= L'A' && c <= L'Z' ? c + (L'a' - L'A') : c; };
                if (lower(actual[i]) != lower(wanted[i])) { same = false; break; }
            }
            if (same) { current /= entry.path().filename(); found = true; break; }
        }
        if (!found) return path;
    }
    return current;
}
FILE* OpenDisk(const std::wstring& path, const char* mode) {
#ifdef _WIN32
    const auto wide_mode = Wide(mode);
    FILE* file = _wfopen(ResolveCase(Path(path)).c_str(), wide_mode.c_str());
    struct _stat64 st;
    if (file && (_fstat64(_fileno(file), &st) || (st.st_mode & _S_IFMT) == _S_IFDIR)) {
        std::fclose(file); return nullptr;
    }
#else
    FILE* file = std::fopen(ResolveCase(Path(path)).c_str(), mode);
    struct stat st;
    if (file && (fstat(fileno(file), &st) || S_ISDIR(st.st_mode))) {
        std::fclose(file); return nullptr;
    }
#endif
    return file;
}
std::int64_t Tell(FILE* f) {
#ifdef _WIN32
    const auto result = _ftelli64(f);
#else
    const auto result = ftello(f);
#endif
    if (result < 0) ERROR_S(L"Error getting file pointer");
    return result;
}
void Seek(FILE* f, std::int64_t position) {
#ifdef _WIN32
    const auto result = _fseeki64(f, position, SEEK_SET);
#else
    static_assert(sizeof(off_t) >= 8);
    const auto result = fseeko(f, static_cast<off_t>(position), SEEK_SET);
#endif
    if (result) ERROR_S(L"Error setting file pointer");
    std::clearerr(f);
}
std::int64_t DiskSize(FILE* f) {
#ifdef _WIN32
    struct _stat64 st;
    if (_fstat64(_fileno(f), &st)) ERROR_S(L"Error getting file size");
#else
    struct stat st;
    if (fstat(fileno(f), &st)) ERROR_S(L"Error getting file size");
#endif
    if (st.st_size < 0) ERROR_S(L"Negative file size");
    return st.st_size;
}
bool Exists(const std::wstring& path) {
    std::error_code error;
    return fs::exists(ResolveCase(Path(path)), error) && !error;
}
std::wstring Fold(std::wstring text) {
    for (auto& ch : text) if (ch >= L'A' && ch <= L'Z') ch += L'a' - L'A';
    return text;
}
bool Match(std::wstring_view name, std::wstring_view pattern) {
    if (pattern == L"*.*") pattern = L"*";
    size_t n = 0, p = 0, star = pattern.npos, back = 0;
    const auto fold = [](wchar_t ch) { return ch >= L'A' && ch <= L'Z' ? ch + (L'a' - L'A') : ch; };
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == L'?' || fold(pattern[p]) == fold(name[n]))) { ++n; ++p; }
        else if (p < pattern.size() && pattern[p] == L'*') { star = p++; back = n; }
        else if (star != pattern.npos) { p = star + 1; n = ++back; }
        else return false;
    }
    while (p < pattern.size() && pattern[p] == L'*') ++p;
    return p == pattern.size();
}
std::vector<std::wstring> Extensions(const wchar* text) {
    std::vector<std::wstring> list;
    if (!text) return list;
    std::wstring_view rest(text);
    while (!rest.empty()) {
        const auto end = rest.find(L'~');
        const auto part = rest.substr(0, end);
        if (!part.empty()) list.push_back(Fold(std::wstring(part)));
        if (end == rest.npos) break;
        rest.remove_prefix(end + 1);
    }
    return list;
}
} // namespace

struct CFile::Impl {
    std::wstring name;
    FILE* file = nullptr;
    std::shared_ptr<Package> package;
    uint32_t entry = 0;
    int opens = 0;
    Impl() { (void)Packages(); }
    void Reset() {
        if (package) {
            auto& registry = Packages();
            std::lock_guard lock(registry.mutex);
            package->CloseEntry(entry);
            package.reset(); entry = 0;
            --registry.active;
        }
        if (file) { std::fclose(file); file = nullptr; }
        opens = 0;
    }
    void Reopen() {
        if (opens == INT_MAX) ERROR_S(L"File open count overflow");
        ++opens;
    }
    void RequireOpen() const { if (!opens) ERROR_S(L"File is not open"); }
};
CFile::CFile() : impl_(std::make_unique<Impl>()) {}
CFile::CFile(const std::wstring& filename) : CFile() { Init(filename); }
CFile::CFile(const wchar* filename) : CFile() { Init(filename); }
CFile::CFile(const wchar* filename, int length) : CFile() { Init(filename, length); }
CFile::~CFile() { impl_->Reset(); }
void CFile::Clear() { impl_->Reset(); impl_->name.clear(); }
void CFile::Init(const wchar* name, int length) {
    if (!name || length < 0) ERROR_S(L"Invalid file name");
    auto copy = Normalize(std::wstring(name, static_cast<size_t>(length)));
    (void)Utf8(copy); // Validate the entire path before closing the old file.
    impl_->Reset(); impl_->name = std::move(copy);
}
void CFile::Init(const wchar* name) {
    if (!name) ERROR_S(L"Invalid file name");
    Init(std::wstring(name));
}
void CFile::Init(const std::wstring& name) {
    if (name.size() > INT_MAX) ERROR_S(L"File name is too long");
    Init(name.c_str(), static_cast<int>(name.size()));
}
bool CFile::IsOpen() { return impl_->opens > 0; }
void CFile::StaticInit() { ReleasePackFiles(); }
void CFile::AddPackFile(const wchar* name) {
    if (!name) ERROR_S(L"Invalid package name");
    auto path = Normalize(name); (void)Utf8(path);
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    registry.slots.push_back({std::move(path), {}});
}
void CFile::OpenPackFiles() {
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    if (registry.active) ERROR_S(L"Cannot reopen packages with active files");
    std::vector<std::shared_ptr<Package>> opened;
    for (const auto& slot : registry.slots) {
        auto package = std::make_shared<Package>(); std::string error;
        if (!package->OpenUtf8(Utf8(slot.path), &error)) {
            registry.slots.clear();
            ERROR_S(L"Error opening package collection");
        }
        opened.push_back(std::move(package));
    }
    for (size_t i = 0; i < opened.size(); ++i) registry.slots[i].package = std::move(opened[i]);
}
void CFile::ReleasePackFiles() {
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    if (registry.active) ERROR_S(L"Cannot release packages with active files");
    registry.slots.clear();
}
void CFile::Open(dword /* shareMode */) {
    if (impl_->opens) { impl_->Reopen(); return; }
    impl_->file = OpenDisk(impl_->name, "r+b");
    if (!impl_->file) ERROR_S2(L"Error open file: ", impl_->name.c_str());
    impl_->opens = 1;
}
void CFile::OpenRead(dword /* shareMode */) {
    if (impl_->opens) { impl_->Reopen(); return; }
    impl_->file = OpenDisk(impl_->name, "rb");
    if (impl_->file) { impl_->opens = 1; return; }
    const auto name = Utf8(impl_->name);
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    for (const auto& slot : registry.slots) {
        if (!slot.package) continue;
        std::string error;
        const auto handle = slot.package->OpenEntryByPath(name, &error);
        if (handle != 0) {
            impl_->package = slot.package; impl_->entry = handle; impl_->opens = 1;
            ++registry.active; return;
        }
    }
    ERROR_S2(L"Error open file: ", impl_->name.c_str());
}
bool CFile::OpenReadNE(dword /* shareMode */) {
    if (impl_->opens) { impl_->Reopen(); return true; }
    impl_->file = OpenDisk(impl_->name, "rb"); // Original NE variant is disk-only.
    if (!impl_->file) return false;
    impl_->opens = 1; return true;
}
bool CFile::CreateNE(dword /* shareMode */) {
    if (impl_->opens) { impl_->Reopen(); return true; }
    impl_->file = OpenDisk(impl_->name, "w+b");
    if (!impl_->file) return false;
    impl_->opens = 1; return true;
}
void CFile::Create(dword shareMode) {
    if (!CreateNE(shareMode)) ERROR_S2(L"Error creating file: ", impl_->name.c_str());
}
void CFile::Close() {
    if (impl_->opens > 1) --impl_->opens;
    else impl_->Reset();
}
std::int64_t CFile::SizeFull() const {
    impl_->RequireOpen();
    if (impl_->file) return DiskSize(impl_->file);
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    const auto size = impl_->package->GetEntrySize(impl_->entry);
    if (size > INT64_MAX) ERROR_S(L"File size exceeds int64");
    return static_cast<std::int64_t>(size);
}
dword CFile::Size() const {
    const auto size = SizeFull();
    if (size >= UINT32_MAX) ERROR_S(L"File size exceeds 32-bit API");
    return static_cast<dword>(size);
}
std::int64_t CFile::PointerFull() const {
    impl_->RequireOpen();
    if (impl_->file) return Tell(impl_->file);
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    const auto offset = impl_->package->GetEntryPosition(impl_->entry);
    if (offset > INT64_MAX) ERROR_S(L"File offset exceeds int64");
    return static_cast<std::int64_t>(offset);
}
dword CFile::Pointer() const {
    const auto offset = PointerFull();
    if (offset >= UINT32_MAX) ERROR_S(L"File offset exceeds 32-bit API");
    return static_cast<dword>(offset);
}
void CFile::PointerFull(std::int64_t offset, int from) const {
    impl_->RequireOpen();
    if (from < 0 || from > 2) ERROR_S(L"Invalid file seek origin");
    const auto base = from == 0 ? 0 : from == 1 ? PointerFull() : SizeFull();
    if ((offset < 0 && offset < -base) || (offset > 0 && offset > INT64_MAX - base))
        ERROR_S(L"Invalid file seek offset");
    // Legacy MatrixLib package seeks use a positive distance back from EOF;
    // disk FILE_END follows signed stdio/Win32 offsets. Preserve that boundary.
    if (impl_->package && from == 2 && (offset < 0 || offset > base))
        ERROR_S(L"Invalid package end-relative offset");
    const auto position = impl_->package && from == 2 ? base - offset : base + offset;
    if (impl_->file) { Seek(impl_->file, position); return; }
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    std::string error;
    if (!impl_->package->SeekEntry(impl_->entry, static_cast<uint64_t>(position), &error))
        ERROR_S(L"Error seeking package file");
}
void CFile::Pointer(dword offset, int from) const {
    PointerFull(from == 0 || (impl_->package && from == 2) ? std::int64_t(offset) : std::int64_t(static_cast<std::int32_t>(offset)), from);
}
void CFile::Read(void* buffer, dword bytes) {
    impl_->RequireOpen();
    if (!bytes) return;
    if (!buffer) ERROR_S(L"NULL file read buffer");
    size_t count;
    if (impl_->file) count = std::fread(buffer, 1, bytes, impl_->file);
    else {
        auto& registry = Packages(); std::lock_guard lock(registry.mutex);
        std::string error;
        count = impl_->package->ReadEntry(impl_->entry, buffer, bytes, &error);
    }
    if (count != bytes) ERROR_S2(L"Error read file: ", impl_->name.c_str());
}
void CFile::Write(void* buffer, dword bytes) {
    if (!bytes) return;
    impl_->RequireOpen();
    if (!impl_->file || !buffer) ERROR_S(L"File is not writable");
    if (std::fwrite(buffer, 1, bytes, impl_->file) != bytes || std::fflush(impl_->file))
        ERROR_S2(L"Error write file: ", impl_->name.c_str());
}
bool CFile::FileExist(std::wstring& output, const wchar* name, const wchar* extensions, bool withpar) {
    if (!name) return false;
    std::wstring full(name);
    if (full.starts_with(L"./") || full.starts_with(L".\\")) full.erase(0, 2);
    const auto query = full.find(L'?');
    const auto plain = full.substr(0, query);
    const auto suffix = withpar && query != full.npos ? full.substr(query) : std::wstring{};
    const auto list = Extensions(extensions);
    if (Exists(Normalize(plain))) { output = plain + suffix; return true; }
    const auto path = Path(Normalize(plain));
    const auto parent = path.parent_path().empty() ? fs::path(".") : ResolveCase(path.parent_path());
    const auto prefix = PathWide(path.filename()) + L".";
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(parent, error)) {
        const auto filename = PathWide(entry.path().filename());
        if (!Fold(filename).starts_with(Fold(prefix))) continue;
        const auto dot = filename.rfind(L'.');
        if (extensions && (dot == filename.npos || std::find(list.begin(), list.end(), Fold(filename.substr(dot + 1))) == list.end())) continue;
        const auto slash = plain.find_last_of(L"/\\");
        output = (slash == plain.npos ? std::wstring{} : plain.substr(0, slash + 1)) + filename + suffix;
        return true;
    }
    auto& registry = Packages(); std::lock_guard lock(registry.mutex);
    const auto present = [&](const std::wstring& candidate) {
        const auto key = Utf8(Normalize(candidate));
        for (const auto& slot : registry.slots)
            if (slot.package && slot.package->Resolve(key)) return true;
        return false;
    };
    if (present(plain)) { output = plain + suffix; return true; }
    for (const auto& ext : list) {
        const auto candidate = plain + L"." + ext;
        if (present(candidate)) { output = candidate + suffix; return true; }
    }
    return false;
}
void CFile::FindFiles(const std::wstring& folder, const wchar* pattern, ENUM_FILES callback, dword user) {
    if (!pattern || !callback) ERROR_S(L"Invalid file enumeration callback");
    std::error_code error;
    const auto root = Path(Normalize(folder));
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, error)) {
        if (entry.is_regular_file(error) && Match(PathWide(entry.path().filename()), pattern))
            callback(PathWide(entry.path()), user);
    }
}
} // namespace Base

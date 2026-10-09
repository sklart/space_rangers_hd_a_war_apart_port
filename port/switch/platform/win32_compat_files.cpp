#include "win32_compat_files.hpp"

#include "game_path.hpp"
#include "e2e_file_search.hpp"
#include "e2e_stage.hpp"
#include "user_root.hpp"
#include "win32_compat_filetime.hpp"
#include "win32_handles.hpp"
#include "types/Windows_group.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace srhd_awa::platform::win32_compat {
namespace {
constexpr std::uint32_t kInvalid = 0xffffffffu;
constexpr std::uint32_t kGenericRead = 0x80000000u;
constexpr std::uint32_t kGenericWrite = 0x40000000u;
constexpr std::uint32_t kShareRead = 1;
constexpr std::uint32_t kShareWrite = 2;
constexpr std::uint32_t kCreateNew = 1;
constexpr std::uint32_t kCreateAlways = 2;
constexpr std::uint32_t kOpenExisting = 3;
constexpr std::uint32_t kOpenAlways = 4;
constexpr std::uint32_t kTruncateExisting = 5;

struct FileObject {
  std::FILE* file;
  std::uint32_t access;
  std::uint32_t share;
  FileObject(std::FILE* value, std::uint32_t access_mode,
             std::uint32_t share_mode)
      : file(value), access(access_mode), share(share_mode) {}
  ~FileObject() { if (file) std::fclose(file); }
};
std::mutex g_open_files_mutex;
std::unordered_map<std::string, std::vector<std::weak_ptr<FileObject>>> g_open_files;
struct FindObject {
  std::vector<std::filesystem::directory_entry> entries;
  std::size_t next = 0;
};

std::string NarrowPath(const char16_t* wide);

std::string Path(const char* input, bool writing) {
  if (!input || !*input) return {};
  std::string normalized(input);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  // The game root and the portable user root are selected by existing path
  // adapters. An absolute path produced by either adapter stays absolute.
  if (normalized.starts_with("sdmc:/") || std::filesystem::path(normalized).is_absolute())
    return normalized;
  if (!writing) {
    const auto user = user_root::ResolveConfigPath(normalized);
    if (!user.empty() && std::filesystem::exists(user)) return user;
  }
  if (writing) return user_root::ResolveConfigPath(normalized);
  const auto game = game_path::Resolve(normalized);
  return game.empty() ? normalized : game;
}

std::string ShareKey(const std::string& path) {
  std::string key = std::filesystem::path(path).lexically_normal().generic_string();
  for (char& c : key)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return key;
}

std::uint32_t OpenFile(const char* input, std::uint32_t access,
                       std::uint32_t share,
                       std::uint32_t disposition) {
  const bool writing = (access & kGenericWrite) != 0;
  const auto path = Path(input, writing);
  if (path.empty() || !(access & (kGenericRead | kGenericWrite))) {
    SetLastError(kErrorInvalidParameter);
    return kInvalid;
  }
  std::lock_guard lock(g_open_files_mutex);
  auto& open = g_open_files[ShareKey(path)];
  std::erase_if(open, [](const auto& handle) { return handle.expired(); });
  for (const auto& weak : open) {
    const auto peer = weak.lock();
    if (!peer) continue;
    if (((access & kGenericRead) && !(peer->share & kShareRead)) ||
        ((access & kGenericWrite) && !(peer->share & kShareWrite)) ||
        ((peer->access & kGenericRead) && !(share & kShareRead)) ||
        ((peer->access & kGenericWrite) && !(share & kShareWrite))) {
      SetLastError(kErrorSharingViolation);
      return kInvalid;
    }
  }
  const bool exists = std::filesystem::is_regular_file(path);
  if (disposition == kCreateNew && exists) {
    SetLastError(kErrorAlreadyExists);
    return kInvalid;
  }
  if ((disposition == kOpenExisting || disposition == kTruncateExisting) && !exists) {
    SetLastError(kErrorFileNotFound);
    return kInvalid;
  }
  if (disposition < kCreateNew || disposition > kTruncateExisting ||
      ((disposition != kOpenExisting) && !writing)) {
    SetLastError(kErrorInvalidParameter);
    return kInvalid;
  }
  const char* mode = nullptr;
  if (disposition == kCreateAlways || disposition == kCreateNew ||
      disposition == kTruncateExisting) mode = (access & kGenericRead) ? "w+b" : "wb";
  else if (writing) mode = exists ? "r+b" : "w+b";
  else mode = "rb";
  auto* file = std::fopen(path.c_str(), mode);
  if (!file) {
    SetLastError(std::filesystem::exists(std::filesystem::path(path).parent_path())
                     ? kErrorAccessDenied : kErrorPathNotFound);
    return kInvalid;
  }
  auto object = std::make_shared<FileObject>(file, access, share);
  const auto token = Handles().Allocate(HandleType::File, object);
  if (!token) return kInvalid;
  open.push_back(object);
  SetLastError(exists && disposition == kOpenAlways ? kErrorAlreadyExists : kErrorSuccess);
  return token;
}

std::uint32_t CreateFileAThunk(std::uint8_t* path, std::uint32_t access,
    std::uint32_t share, void*, std::uint32_t disposition, std::uint32_t, std::uint32_t) {
  return OpenFile(reinterpret_cast<const char*>(path), access, share, disposition);
}

std::uint32_t CreateFileWThunk(char16_t* path, std::uint32_t access,
    std::uint32_t share, void* security, std::uint32_t disposition,
    std::uint32_t flags, std::uint32_t pattern) {
  if (!path) { SetLastError(kErrorInvalidParameter); return kInvalid; }
  const auto narrow = NarrowPath(path);
  (void)security;
  (void)flags;
  (void)pattern;
  return OpenFile(narrow.c_str(), access, share, disposition);
}

std::shared_ptr<FileObject> LookupFile(std::uint32_t token) {
  return std::static_pointer_cast<FileObject>(Handles().Lookup(token, HandleType::File));
}

std::int32_t ReadFileThunk(std::uint32_t token, void* buffer, std::uint32_t count,
                           std::uint32_t* done, void* overlapped) {
  if (done) *done = 0;
  if (!done || (!buffer && count) || overlapped) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  const auto file = LookupFile(token);
  if (!file) return 0;
  *done = static_cast<std::uint32_t>(std::fread(buffer, 1, count, file->file));
  if (std::ferror(file->file)) { SetLastError(kErrorAccessDenied); return 0; }
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t WriteFileThunk(std::uint32_t token, const void* buffer,
                            std::uint32_t count, std::uint32_t* done,
                            void* overlapped) {
  if (done) *done = 0;
  if (!done || (!buffer && count) || overlapped) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  const auto file = LookupFile(token);
  if (!file) return 0;
  *done = static_cast<std::uint32_t>(std::fwrite(buffer, 1, count, file->file));
  if (*done != count) { SetLastError(kErrorAccessDenied); return 0; }
  SetLastError(kErrorSuccess);
  return 1;
}

std::uint32_t SetFilePointerThunk(std::uint32_t token, std::int32_t low,
                                  std::int32_t* high, std::uint32_t origin) {
  const auto file = LookupFile(token);
  if (!file) return kInvalid;
  if (high || origin > 2) { SetLastError(kErrorInvalidParameter); return kInvalid; }
  const int base = origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END;
  if (std::fseek(file->file, low, base) != 0) {
    SetLastError(kErrorInvalidParameter);
    return kInvalid;
  }
  const auto position = std::ftell(file->file);
  if (position < 0) { SetLastError(kErrorAccessDenied); return kInvalid; }
  SetLastError(kErrorSuccess);
  return static_cast<std::uint32_t>(position);
}

std::uint32_t GetFileSizeThunk(std::uint32_t token, std::uint32_t* high) {
  const auto file = LookupFile(token);
  if (!file) return kInvalid;
  const auto position = std::ftell(file->file);
  if (position < 0 || std::fseek(file->file, 0, SEEK_END) != 0) {
    SetLastError(kErrorAccessDenied);
    return kInvalid;
  }
  const auto size = std::ftell(file->file);
  std::fseek(file->file, position, SEEK_SET);
  if (size < 0) { SetLastError(kErrorAccessDenied); return kInvalid; }
  if (high) *high = 0;
  SetLastError(kErrorSuccess);
  return static_cast<std::uint32_t>(size);
}

std::uint32_t GetFileAttributesAThunk(std::uint8_t* name) {
  const auto path = Path(reinterpret_cast<const char*>(name), false);
  if (path.empty()) { SetLastError(kErrorInvalidParameter); return kInvalid; }
  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  if (error || !std::filesystem::exists(status)) {
    SetLastError(kErrorFileNotFound);
    return kInvalid;
  }
  SetLastError(kErrorSuccess);
  return std::filesystem::is_directory(status) ? 0x10u : 0x80u;
}

std::int32_t DeleteFileAThunk(std::uint8_t* name) {
  const auto path = Path(reinterpret_cast<const char*>(name), true);
  if (path.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  std::error_code error;
  const bool removed = std::filesystem::remove(path, error);
  SetLastError(error ? kErrorAccessDenied : removed ? kErrorSuccess : kErrorFileNotFound);
  return removed ? 1 : 0;
}

template <class Char, class Data>
bool FillFindData(const std::filesystem::directory_entry& entry, Data* output) {
  if (!output) { SetLastError(kErrorInvalidParameter); return false; }
  *output = {};
  std::error_code error;
  const bool directory = entry.is_directory(error);
  if (error) { SetLastError(kErrorAccessDenied); return false; }
  output->dwFileAttributes = directory ? 0x10u : 0x80u;
  const auto size = directory ? 0u : entry.file_size(error);
  if (error) { SetLastError(kErrorAccessDenied); return false; }
  output->nFileSizeLow = static_cast<std::uint32_t>(size);
  output->nFileSizeHigh = static_cast<std::uint32_t>(size >> 32);
  const auto write = entry.last_write_time(error);
  if (!error) {
    const auto system = std::chrono::file_clock::to_sys(write);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        system.time_since_epoch()).count();
    output->ftLastWriteTime = UnixMillisToFileTime(milliseconds);
    output->ftCreationTime = output->ftLastWriteTime;
    output->ftLastAccessTime = output->ftLastWriteTime;
  }
  if constexpr (std::is_same_v<Char, char16_t>) {
    const auto name = entry.path().filename().u16string();
    const auto count = std::min(name.size(), std::size(output->cFileName.elements) - 1);
    for (std::size_t i = 0; i < count; ++i) output->cFileName.elements[i] = name[i];
  } else {
    const auto name = entry.path().filename().string();
    const auto count = std::min(name.size(), std::size(output->cFileName.elements) - 1);
    for (std::size_t i = 0; i < count; ++i)
      output->cFileName.elements[i] = static_cast<Char>(static_cast<unsigned char>(name[i]));
  }
  SetLastError(kErrorSuccess);
  return true;
}

template <class Char, class Data>
std::uint32_t FindFirst(const Char* pattern, Data* output) {
  if (!pattern || !output) { SetLastError(kErrorInvalidParameter); return kInvalid; }
  std::string narrow;
  if constexpr (std::is_same_v<Char, char16_t>) narrow = NarrowPath(pattern);
  else for (const Char* p = pattern; *p; ++p) narrow.push_back(static_cast<char>(*p));
  std::replace(narrow.begin(), narrow.end(), '\\', '/');
  const auto requested = std::filesystem::path(narrow);
  const auto mask = requested.filename().string();
  const auto directory = requested.has_parent_path()
      ? Path(requested.parent_path().generic_string().c_str(), false)
      : Path(".", false);
  if (directory.empty()) { SetLastError(kErrorPathNotFound); return kInvalid; }
  std::error_code error;
  auto found = std::make_shared<FindObject>();
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (error) break;
    const auto name = entry.path().filename().string();
    if (e2e_file_search::MatchPattern(mask.c_str(), name.c_str()))
      found->entries.push_back(entry);
  }
  if (error) { SetLastError(kErrorPathNotFound); return kInvalid; }
  if (found->entries.empty()) { SetLastError(kErrorFileNotFound); return kInvalid; }
  if (!FillFindData<Char>(found->entries.front(), output)) return kInvalid;
  found->next = 1;
  return Handles().Allocate(HandleType::Find, std::move(found));
}

template <class Char, class Data>
std::int32_t FindNext(std::uint32_t token, Data* output) {
  const auto found = std::static_pointer_cast<FindObject>(
      Handles().Lookup(token, HandleType::Find));
  if (!found) return 0;
  if (found->next >= found->entries.size()) {
    SetLastError(kErrorNoMoreFiles);
    return 0;
  }
  return FillFindData<Char>(found->entries[found->next++], output) ? 1 : 0;
}

std::uint32_t FindFirstAThunk(std::uint8_t* pattern, Windows::TWin32FindDataA* data) {
  return FindFirst(pattern, data);
}
std::uint32_t FindFirstWThunk(char16_t* pattern, WindowsSdk::TWin32FindDataW* data) {
  return FindFirst(pattern, data);
}
std::int32_t FindNextAThunk(std::uint32_t token, Windows::TWin32FindDataA* data) {
  return FindNext<std::uint8_t>(token, data);
}
std::int32_t FindNextWThunk(std::uint32_t token, WindowsSdk::TWin32FindDataW* data) {
  return FindNext<char16_t>(token, data);
}
std::int32_t FindCloseThunk(std::uint32_t token) {
  return Handles().Close(token, HandleType::Find) ? 1 : 0;
}

std::int32_t CreateDirectoryAThunk(std::uint8_t* name, void*) {
  const auto path = Path(reinterpret_cast<const char*>(name), true);
  if (path.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  std::error_code error;
  const bool created = std::filesystem::create_directory(path, error);
  SetLastError(error ? kErrorAccessDenied : created ? kErrorSuccess : kErrorAlreadyExists);
  return created ? 1 : 0;
}

std::uint32_t GetCurrentDirectoryAThunk(std::uint32_t capacity, std::uint8_t* output) {
  std::error_code error;
  auto path = std::filesystem::current_path(error).generic_string();
  if (error) { SetLastError(kErrorAccessDenied); return 0; }
  const auto needed = static_cast<std::uint32_t>(path.size() + 1);
  if (capacity < needed || !output) { SetLastError(kErrorInvalidParameter); return needed; }
  std::memcpy(output, path.c_str(), needed);
  SetLastError(kErrorSuccess);
  return needed - 1;
}

std::int32_t SetCurrentDirectoryAThunk(std::uint8_t* name) {
  const auto path = Path(reinterpret_cast<const char*>(name), false);
  if (path.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  std::error_code error;
  std::filesystem::current_path(path, error);
  SetLastError(error ? kErrorPathNotFound : kErrorSuccess);
  return error ? 0 : 1;
}

std::int32_t CopyFileAThunk(std::uint8_t* source, std::uint8_t* destination,
                            std::int32_t fail_if_exists) {
  const auto from = Path(reinterpret_cast<const char*>(source), false);
  const auto to = Path(reinterpret_cast<const char*>(destination), true);
  if (from.empty() || to.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  std::error_code error;
  const bool copied = std::filesystem::copy_file(from, to,
      fail_if_exists ? std::filesystem::copy_options::none
                     : std::filesystem::copy_options::overwrite_existing, error);
  SetLastError(error ? kErrorAccessDenied : copied ? kErrorSuccess : kErrorAlreadyExists);
  return copied ? 1 : 0;
}

std::string NarrowPath(const char16_t* wide) {
  if (!wide) return {};
  std::string result;
  for (; *wide; ++wide) {
    if (*wide < 0x80) result.push_back(static_cast<char>(*wide));
    else if (*wide < 0x800) {
      result.push_back(static_cast<char>(0xc0 | (*wide >> 6)));
      result.push_back(static_cast<char>(0x80 | (*wide & 0x3f)));
    } else {
      result.push_back(static_cast<char>(0xe0 | (*wide >> 12)));
      result.push_back(static_cast<char>(0x80 | ((*wide >> 6) & 0x3f)));
      result.push_back(static_cast<char>(0x80 | (*wide & 0x3f)));
    }
  }
  return result;
}

std::int32_t CopyFileWThunk(char16_t* source, char16_t* destination,
                            std::int32_t fail_if_exists) {
  const auto from = NarrowPath(source), to = NarrowPath(destination);
  if (from.empty() || to.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  return CopyFileAThunk(reinterpret_cast<std::uint8_t*>(const_cast<char*>(from.c_str())),
                        reinterpret_cast<std::uint8_t*>(const_cast<char*>(to.c_str())),
                        fail_if_exists);
}

std::int32_t MoveFileWThunk(char16_t* source, char16_t* destination) {
  const auto from = Path(NarrowPath(source).c_str(), false);
  const auto to = Path(NarrowPath(destination).c_str(), true);
  if (from.empty() || to.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  std::error_code error;
  std::filesystem::rename(from, to, error);
  SetLastError(error ? kErrorAccessDenied : kErrorSuccess);
  return error ? 0 : 1;
}

std::int32_t SetFileAttributesAThunk(std::uint8_t* name, std::uint32_t attributes) {
  const auto path = Path(reinterpret_cast<const char*>(name), true);
  if (path.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  std::error_code error;
  if (!std::filesystem::exists(path, error) || error) {
    SetLastError(kErrorFileNotFound);
    return 0;
  }
  // The source only clears READONLY. Other Windows attribute bits have no
  // portable on-disk representation in the Switch save root.
  if (attributes == 0x80u || attributes == 0u)
    std::filesystem::permissions(path, std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add, error);
  else if (attributes == 0x1u)
    std::filesystem::permissions(path, std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::remove, error);
  else { SetLastError(kErrorInvalidParameter); return 0; }
  SetLastError(error ? kErrorAccessDenied : kErrorSuccess);
  return error ? 0 : 1;
}

std::int32_t GetVolumeInformationAThunk(std::uint8_t*, std::uint8_t* volume,
    std::uint32_t volume_capacity, std::uint32_t* serial,
    std::uint32_t* max_component, std::uint32_t* flags,
    std::uint8_t* filesystem, std::uint32_t filesystem_capacity) {
  constexpr const char* kVolume = "SwitchGame";
  constexpr const char* kFilesystem = "SDMC";
  if ((volume && volume_capacity <= std::strlen(kVolume)) ||
      (filesystem && filesystem_capacity <= std::strlen(kFilesystem))) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  if (volume) std::memcpy(volume, kVolume, std::strlen(kVolume) + 1);
  if (filesystem) std::memcpy(filesystem, kFilesystem, std::strlen(kFilesystem) + 1);
  if (serial) {
    std::uint32_t hash = 2166136261u;
    for (const char* p = e2e_stage::kRoot; *p; ++p)
      hash = (hash ^ static_cast<std::uint8_t>(*p)) * 16777619u;
    *serial = hash;
  }
  if (max_component) *max_component = 255;
  if (flags) *flags = 0;
  SetLastError(kErrorSuccess);
  return 1;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

bool CloseFileHandle(std::uint32_t token) {
  const auto file = LookupFile(token);
  if (!file || !Handles().Close(token, HandleType::File)) return false;
  // Any in-flight lookup holds a reference until its operation ends.
  return true;
}

ImportAddress ResolveFileImport(std::string_view dll, std::string_view symbol) {
  if (dll != "kernel32.dll") return nullptr;
  if (symbol == "CreateFileA") return Address(&CreateFileAThunk);
  if (symbol == "CreateFileW") return Address(&CreateFileWThunk);
  if (symbol == "ReadFile") return Address(&ReadFileThunk);
  if (symbol == "WriteFile") return Address(&WriteFileThunk);
  if (symbol == "SetFilePointer") return Address(&SetFilePointerThunk);
  if (symbol == "GetFileSize") return Address(&GetFileSizeThunk);
  if (symbol == "GetFileAttributesA") return Address(&GetFileAttributesAThunk);
  if (symbol == "DeleteFileA") return Address(&DeleteFileAThunk);
  if (symbol == "FindFirstFileA") return Address(&FindFirstAThunk);
  if (symbol == "FindFirstFileW") return Address(&FindFirstWThunk);
  if (symbol == "FindNextFileA") return Address(&FindNextAThunk);
  if (symbol == "FindNextFileW") return Address(&FindNextWThunk);
  if (symbol == "FindClose") return Address(&FindCloseThunk);
  if (symbol == "CreateDirectoryA") return Address(&CreateDirectoryAThunk);
  if (symbol == "GetCurrentDirectoryA") return Address(&GetCurrentDirectoryAThunk);
  if (symbol == "SetCurrentDirectoryA") return Address(&SetCurrentDirectoryAThunk);
  if (symbol == "CopyFileA") return Address(&CopyFileAThunk);
  if (symbol == "CopyFileW") return Address(&CopyFileWThunk);
  if (symbol == "MoveFileW") return Address(&MoveFileWThunk);
  if (symbol == "SetFileAttributesA") return Address(&SetFileAttributesAThunk);
  if (symbol == "GetVolumeInformationA") return Address(&GetVolumeInformationAThunk);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat

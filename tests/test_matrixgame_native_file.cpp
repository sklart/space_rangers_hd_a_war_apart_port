// Original CFile API fixtures. No game UI, campaign or battle is simulated.
#include "CFile.hpp"
#include "CBuf.hpp"
#include <algorithm>
#include <chrono>
#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#ifdef _MSC_VER
#include <cstdlib>
#include <crtdbg.h>
#endif
#ifdef NDEBUG
#error File contract requires active assertions
#endif
#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    std::fflush(stderr); std::exit(3); } } while (0)
namespace fs = std::filesystem;
static_assert(sizeof(dword) == 4);
std::wstring wide(const fs::path& p) {
#ifdef _WIN32
    return p.wstring();
#else
    const auto utf8 = p.native();
    // Fixture root is deliberately ASCII; Unicode suffix is supplied as wchar.
    return std::wstring(utf8.begin(), utf8.end());
#endif
}
template<class Fn> void rejects(Fn action) {
    bool rejected = false;
    try { action(); } catch (const Base::CException&) { rejected = true; }
    CHECK(rejected);
}
std::vector<uint8_t> bytes(const Base::CBuf& b) {
    const auto p = static_cast<const uint8_t*>(b.Get());
    return b.Len() ? std::vector<uint8_t>(p, p + b.Len()) : std::vector<uint8_t>{};
}
std::vector<std::wstring> enumerated;
void found(const std::wstring& name, dword user) {
    CHECK(user == 13579); enumerated.push_back(name);
}
std::int64_t position(Base::CFile& file) {
#ifdef MATRIXGAME_ORIGINAL_FILE_REFERENCE
    // Original disk PointerFull getter passes an uninitialized LONG to Win32.
    return file.Pointer();
#else
    return file.PointerFull();
#endif
}
uint8_t expected(size_t offset) { return static_cast<uint8_t>((offset * 17) % 251); }
int main(int argc, char** argv) {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    CHECK(argc >= 3);
    const auto root = fs::absolute(argv[1]);
    CHECK(fs::is_directory(root));
    const auto pkg = wide(fs::absolute(argv[2]));
    Base::CFile::StaticInit();
    const auto plain = wide(root / "plain.bin");
    std::array<uint8_t,10> data{0,1,2,3,4,5,6,7,8,255};
    {
        Base::CFile f(plain);
        CHECK(!f.IsOpen()); f.Create(); CHECK(f.IsOpen());
        f.Write(data.data(), static_cast<dword>(data.size()));
        CHECK(f.Size() == data.size() && f.SizeFull() == data.size());
        CHECK(position(f) == data.size());
        f.Pointer(0); std::array<uint8_t,10> read{}; f.Read(read.data(), static_cast<dword>(read.size())); CHECK(read == data);
        f.PointerFull(-3, 2); CHECK(f.Pointer() == 7); f.Read(read.data(), 3); CHECK(read[0] == 7 && read[2] == 255);
        f.Pointer(0); f.OpenRead(); f.Close(); CHECK(f.IsOpen());
        f.Close(); CHECK(!f.IsOpen()); f.Close(); CHECK(!f.IsOpen());
        CHECK(f.OpenReadNE()); f.Close();
    }
    {
        Base::CFile folder(wide(root));
        CHECK(!folder.OpenReadNE() && !folder.IsOpen());
        rejects([&] { folder.OpenRead(); }); CHECK(!folder.IsOpen());
    }
    {
        Base::CFile f(wide(root / "missing.bin"));
        CHECK(!f.OpenReadNE()); CHECK(!f.IsOpen());
        rejects([&] { f.OpenRead(); }); CHECK(!f.IsOpen());
        rejects([&] { f.Open(); }); CHECK(!f.IsOpen());
        f.Init(plain); f.OpenRead();
        f.Pointer(0); std::array<uint8_t,11> read{};
#ifdef MATRIXGAME_ORIGINAL_FILE_REFERENCE
        // Original CExceptionStr appends a null second argument on ERROR_S.
        f.Read(read.data(), 10);
#else
        rejects([&] { f.Read(read.data(), static_cast<dword>(read.size())); });
#endif
        CHECK(f.Pointer() == 10);
        f.Pointer(0); f.Read(read.data(), 1); CHECK(read[0] == 0);
#ifndef MATRIXGAME_ORIGINAL_FILE_REFERENCE
        rejects([&] { f.PointerFull(-1, 0); }); CHECK(f.Pointer() == 1);
        rejects([&] { f.PointerFull(INT64_MAX, 1); }); CHECK(f.Pointer() == 1);
        rejects([&] { f.PointerFull(0, 3); }); CHECK(f.Pointer() == 1);
#endif
    }
    {
        const auto name = wide(root) + L"/unicode-\u0416\u6c34.bin";
        Base::CBuf original; original.WStr(L"\u0416\u6c34\U0001f680"); original.SaveInFile(name);
        Base::CBuf loaded; loaded.LoadFromFile(name); CHECK(bytes(loaded) == bytes(original));
        loaded.Pointer(0); CHECK(loaded.WStr() == L"\u0416\u6c34\U0001f680");
    }
#if !defined(_WIN32) && !defined(MATRIXGAME_ORIGINAL_FILE_REFERENCE)
    {
        // A sparse >4 GiB fixture lives in host /tmp, never in the installation.
        const auto temp = fs::temp_directory_path() / ("matrixgame-large-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
        {
            Base::CFile f(wide(temp)); f.Create();
            const std::int64_t big = (std::int64_t(1) << 32) + 123;
            f.PointerFull(big); CHECK(f.PointerFull() == big);
            uint8_t mark = 93; f.Write(&mark, 1); CHECK(f.SizeFull() == big + 1);
            CHECK(fs::file_size(temp) == static_cast<uint64_t>(big + 1));
            f.PointerFull(big); uint8_t read = 0; f.Read(&read, 1); CHECK(read == mark);
            rejects([&] { (void)f.Size(); }); rejects([&] { (void)f.Pointer(); });
        }
        fs::remove(temp);
    }
#endif
    fs::create_directories(root / "sub");
    { std::ofstream(root / "chosen.DDS").put('x'); std::ofstream(root / "sub/nested.dds").put('x'); }
    std::wstring filename;
    CHECK(Base::CFile::FileExist(filename, (wide(root / "chosen") + L"?par=1").c_str(), L"dds~png", true));
    CHECK(filename.ends_with(L"chosen.DDS?par=1"));
    auto legacy = wide(root / "chosen");
    std::replace(legacy.begin(), legacy.end(), L'/', L'\\');
    CHECK(Base::CFile::FileExist(filename, (legacy + L"?par=2").c_str(), L"dds~png", true));
    CHECK(filename == legacy + L".DDS?par=2");
    Base::CFile::FindFiles(wide(root), L"*.dds", found, 13579); CHECK(enumerated.size() == 2);
    Base::CFile::AddPackFile(pkg.c_str()); Base::CFile::OpenPackFiles();
    constexpr size_t total = 1100003;
    unsigned reads = 0;
    {
        Base::CFile missing(L"missing-entry"); rejects([&]{ missing.OpenRead(); });
        Base::CFile f(L"music"); CHECK(!f.OpenReadNE()); f.OpenRead();
        CHECK(f.Size() == total && f.SizeFull() == total && f.Pointer() == 0);
        std::array<uint8_t,257> read{};
        uint32_t rng = 0x48a2731u;
        for (unsigned i = 0; i < 300; ++i) {
            rng = rng * 1664525u + 1013904223u;
            const auto position = size_t(rng) % (total - read.size());
            f.PointerFull(static_cast<std::int64_t>(position)); f.Read(read.data(), static_cast<dword>(read.size()));
            for (size_t j = 0; j < read.size(); ++j) CHECK(read[j] == expected(position + j));
            CHECK(f.Pointer() == position + read.size()); ++reads;
        }
        Base::CFile second(L"MUSIC"); second.OpenRead(); CHECK(second.Pointer() == 0);
        second.Read(read.data(), static_cast<dword>(read.size()));
        for (size_t j = 0; j < read.size(); ++j) CHECK(read[j] == expected(j));
#ifndef MATRIXGAME_ORIGINAL_FILE_REFERENCE
        rejects([&]{ Base::CFile::ReleasePackFiles(); });
        rejects([&]{ f.Write(read.data(), 1); });
        rejects([&]{ f.PointerFull(static_cast<std::int64_t>(total + 1)); });
#endif
        f.PointerFull(19, 2); f.Read(read.data(), 19);
        for (size_t j = 0; j < 19; ++j) CHECK(read[j] == expected(total - 19 + j));
        f.Close(); second.Close();
    }
    unsigned concurrent_reads = 0;
#ifndef MATRIXGAME_ORIGINAL_FILE_REFERENCE
    std::atomic<unsigned> completed{0};
    std::atomic<bool> failed{false};
    std::vector<std::thread> readers;
    for (unsigned worker = 0; worker < 4; ++worker) readers.emplace_back([&, worker] {
        try {
            Base::CFile f(L"MUSIC"); f.OpenRead();
            std::array<uint8_t,513> chunk{};
            uint32_t state = 0x2b538a71u + worker;
            for (unsigned i = 0; i < 64; ++i) {
                state = state * 1664525u + 1013904223u;
                const size_t at = size_t(state) % (total - chunk.size());
                f.PointerFull(static_cast<std::int64_t>(at)); f.Read(chunk.data(), static_cast<dword>(chunk.size()));
                for (size_t j = 0; j < chunk.size(); ++j) CHECK(chunk[j] == expected(at + j));
                ++completed;
            }
        } catch (...) { failed = true; }
    });
    for (auto& reader : readers) reader.join();
    CHECK(!failed && completed == 256); concurrent_reads = completed;
#endif
    Base::CFile::ReleasePackFiles();
    // A Unicode package path must use the same bytes on Windows and POSIX.
    const auto unicode_pkg = root / fs::path(u8"\u043f\u0430\u043a\u0435\u0442-\U0001f680.pkg");
    fs::copy_file(fs::absolute(argv[2]), unicode_pkg, fs::copy_options::overwrite_existing);
    const auto unicode_name = wide(root) + L"/\u043f\u0430\u043a\u0435\u0442-\U0001f680.pkg";
    Base::CFile::AddPackFile(unicode_name.c_str()); Base::CFile::OpenPackFiles();
    { Base::CFile f(L"MUSIC"); f.OpenRead(); uint8_t first = 255; f.Read(&first, 1); CHECK(first == expected(0)); }
    Base::CFile::ReleasePackFiles();
#ifndef MATRIXGAME_ORIGINAL_FILE_REFERENCE
    Base::CFile::AddPackFile(wide(fs::absolute(argv[2]).parent_path() / "corrupt.pkg").c_str());
    Base::CFile::OpenPackFiles();
    {
        Base::CFile f(L"MUSIC"); f.OpenRead(); f.PointerFull(static_cast<std::int64_t>(total - 19));
        std::array<uint8_t,19> read{};
        rejects([&]{ f.Read(read.data(), static_cast<dword>(read.size())); });
        f.Pointer(0); f.Read(read.data(), static_cast<dword>(read.size()));
        for (size_t i = 0; i < read.size(); ++i) CHECK(read[i] == expected(i));
    }
    Base::CFile::ReleasePackFiles();
    Base::CFile::AddPackFile(wide(root / "absent.pkg").c_str());
    rejects([&]{ Base::CFile::OpenPackFiles(); });
    Base::CFile::AddPackFile(pkg.c_str()); Base::CFile::OpenPackFiles();
    {
        std::vector<std::unique_ptr<Base::CFile>> opened;
        for (int i = 0; i < 16; ++i) {
            auto file = std::make_unique<Base::CFile>(L"MUSIC"); file->OpenRead(); opened.push_back(std::move(file));
        }
        Base::CFile extra(L"MUSIC"); rejects([&]{ extra.OpenRead(); }); CHECK(!extra.IsOpen());
        opened.pop_back(); extra.OpenRead(); uint8_t first = 255; extra.Read(&first, 1); CHECK(first == 0);
    }
    Base::CFile::ReleasePackFiles();
#endif
    // Real package entry supplied by a separate resource inspection step.
    if (argc == 5) {
        const auto realpkg = wide(fs::absolute(argv[3]));
        const std::string entry(argv[4]);
        Base::CFile::AddPackFile(realpkg.c_str()); Base::CFile::OpenPackFiles();
        {
            Base::CFile f(std::wstring(entry.begin(), entry.end())); f.OpenRead();
            Base::CBuf all; all.LoadFromFile(std::wstring(entry.begin(), entry.end()));
            CHECK(all.Len() == f.Size());
            std::vector<uint8_t> chunk(65537);
            uint32_t hash = 2166136261u;
            for (size_t offset = 0; offset < all.Len();) {
                const auto count = static_cast<dword>(std::min(chunk.size(), all.Len() - offset));
                f.Read(chunk.data(), count);
                const auto source = static_cast<const uint8_t*>(all.Get());
                CHECK(std::equal(chunk.begin(), chunk.begin() + count, source + offset));
                for (size_t i = 0; i < count; ++i) hash = (hash ^ chunk[i]) * 16777619u;
                offset += count;
            }
            std::printf("{\"real_resource_bytes\":%zu,\"fnv32\":\"%08x\"}\n", all.Len(), hash);
        }
        Base::CFile::ReleasePackFiles();
    }
    std::printf("{\"package_reads\":%u,\"concurrent_reads\":%u,\"bytes\":%zu,\"file_io\":\"PASS\"}\n", reads, concurrent_reads, total);
}

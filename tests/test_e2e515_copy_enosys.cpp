#include "e2e4_quicksave_transaction.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace srhd_awa::platform::e2e4_quicksave;
static unsigned chmod_calls{}, sync_calls{};
static bool fail_sync{};
// Export from the executable (-rdynamic) to reproduce libnx's missing
// permission operation in the host's real libstdc++ copy_file implementation.
extern "C" int fchmod(int, mode_t) noexcept {
    ++chmod_calls; errno = ENOSYS; return -1;
}
extern "C" int fsync(int fd) {
    ++sync_calls;
    if (fail_sync) { errno = EIO; return -1; }
    return static_cast<int>(syscall(SYS_fsync, fd));
}
static std::string Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
static bool Write(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), bytes.size());
    out.close();
    return !out.fail();
}
int main(int argc, char** argv) {
    if (argc != 3 || fs::exists(argv[2])) return 2;
    const fs::path source(argv[1]), root(argv[2]);
    std::string detail;
    if (!ValidateSave(source, detail)) return 3;
    const auto bytes = Read(source);
    if (bytes.empty()) return 4;
    fs::create_directories(root);
    std::error_code ec;
    const bool copied = fs::copy_file(source, root / "old-copy.sav", ec);
    if (copied || ec != std::errc::function_not_supported || !chmod_calls ||
        !fs::exists(root / "old-copy.sav") || fs::file_size(root / "old-copy.sav")) {
        std::cerr << "copy_file ENOSYS reproduction failed: " << ec.message() << '\n';
        return 5;
    }
    const auto calls = chmod_calls;
    const auto temp = root / "save.tmp";
    if (!Write(temp, bytes) || !Write(root / "RecoveredSave-1.sav", "")) return 6;
    auto result = PrepareGameTemp(temp.u16string().c_str());
    if (!result.ok || fs::exists(temp) || Read(root / "RecoveredSave-2.sav") != bytes ||
        fs::file_size(root / "RecoveredSave-1.sav") || chmod_calls != calls || !sync_calls)
        return 7;
    // A flush error must still block release of the original temp.
    if (!Write(temp, bytes)) return 8;
    fail_sync = true;
    result = PrepareGameTemp(temp.u16string().c_str());
    fail_sync = false;
    if (result.ok || Read(temp) != bytes || Read(root / "RecoveredSave-3.sav") != bytes)
        return 9;
    result = PrepareGameTemp(temp.u16string().c_str());
    if (!result.ok || fs::exists(temp) || Read(root / "RecoveredSave-4.sav") != bytes ||
        chmod_calls != calls || Read(source) != bytes) return 10;
    std::cout << "PASS real copy_file ENOSYS/empty output reproduced; byte copy, occupied "
                 "recovery slots, fsync failure preservation and retry; source unchanged\n";
}

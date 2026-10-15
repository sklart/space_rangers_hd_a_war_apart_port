#include "e2e4_save_replace.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using srhd_awa::platform::e2e4_save_replace::ReplaceWithBackup;
using srhd_awa::platform::e2e4_save_replace::Status;

static void Write(const fs::path& path, const char* text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    file.close();
    assert(file.good());
}

static std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path root(argv[1]);
    std::error_code error;
    assert(fs::create_directories(root, error) || (!error && fs::is_directory(root)));
    const auto target = root / "slot.sav";
    const auto temp = root / "save.tmp";
    const auto backup = root / "slot.sav.bak";
    const auto backup2 = root / "slot.sav.bak.1";
    const auto rename = [](const fs::path& from, const fs::path& to,
                           std::error_code& result) {
        fs::rename(from, to, result);
        return !result;
    };
    Write(target, "old");
    Write(temp, "new");
    assert(ReplaceWithBackup(temp, target, rename) == Status::success);
    assert(Read(target) == "new" && Read(backup) == "old");

    Write(temp, "newer");
    assert(ReplaceWithBackup(temp, target, rename) == Status::success);
    assert(Read(target) == "newer" && Read(backup) == "old" &&
           Read(backup2) == "new");

    Write(temp, "third");
    int calls = 0;
    const auto fail_install = [&](const fs::path& from, const fs::path& to,
                                  std::error_code& result) {
        if (++calls == 2) {
            result = std::make_error_code(std::errc::permission_denied);
            return false;
        }
        return rename(from, to, result);
    };
    assert(ReplaceWithBackup(temp, target, fail_install) ==
           Status::install_rename_failed);
    assert(Read(target) == "newer" && Read(temp) == "third");
    assert(Read(backup) == "old" && Read(backup2) == "new");

    fs::remove(temp);
    assert(ReplaceWithBackup(temp, target, rename) == Status::missing_or_empty_temp);
    assert(Read(target) == "newer");
    Write(temp, "");
    assert(ReplaceWithBackup(temp, target, rename) == Status::missing_or_empty_temp);
    assert(Read(target) == "newer");

    fs::remove(temp);
    fs::remove(target);
    fs::remove(backup);
    fs::remove(backup2);
    fs::remove(root / "slot.sav.bak.2");
    fs::remove(root);
}

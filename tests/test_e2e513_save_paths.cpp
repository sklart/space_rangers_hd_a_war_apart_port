#include "user_root.hpp"
#include "unicode_path.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace user = srhd_awa::platform::user_root;
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto root = fs::absolute(argv[1]);
    assert(!fs::exists(root));
    fs::create_directories(root / "config/Save");
    std::ofstream(root / "config/Save/AutoSave.sav") << "original-autosave";
    user::SetRoot(root.generic_string());
    assert(user::EnsureLayout());
    const auto request = (root / "config/save/AutoSave.sav").u16string();
    assert(user::ConfigFileExists(request.c_str()));
    assert(user::ResolveConfigPath(request.c_str()) ==
           (root / "config/Save/AutoSave.sav").generic_string());
    const auto unicode = root / "config/Save" / fs::path(u"Ручной Ёж.sav");
    const auto input = (root / "config/save" / fs::path(u"Ручной Ёж.sav")).u16string();
    assert(user::ResolveConfigPath(input.c_str()) == unicode.generic_string());
    std::ofstream(unicode) << "manual-save";
    assert(user::ConfigFileExists(input.c_str()));
    assert(user::ResolveAbsoluteConfigPath(input.c_str()) == unicode.generic_string());
    assert(user::ResolveAbsoluteConfigPath(u"data/resource.gi").empty());
    assert(user::ResolveConfigPath("../outside").empty());
    assert(!fs::exists(root / "config/save"));
    assert(fs::file_size(root / "config/Save/AutoSave.sav") == 17);
    assert(fs::is_empty(root / "save"));
    assert(srhd_awa::platform::PathUtf8(u"Ручной Ёж.sav") ==
           "\xd0\xa0\xd1\x83\xd1\x87\xd0\xbd\xd0\xbe\xd0\xb9 \xd0\x81\xd0\xb6.sav");
    assert(srhd_awa::platform::PathUtf8(u"\U0001f680.sav") == "\xf0\x9f\x9a\x80.sav");
    const char16_t invalid[]{0xd800, 0};
    assert(user::ResolveConfigPath(invalid).empty());
    // A previous candidate may already have created an empty lowercase
    // directory alongside the original Save. It must not hide AutoSave.
    fs::create_directory(root / "config/save");
    assert(user::ResolveConfigPath(request.c_str()) ==
           (root / "config/Save/AutoSave.sav").generic_string());
    std::ofstream(root / "config/save/Other.sav") << "separate-user-slot";
    assert(user::ResolveConfigPath(request.c_str()).empty()); // Ambiguous populated trees.
    assert(fs::file_size(root / "config/Save/AutoSave.sav") == 17);
    std::cout << "PASS UTF-16/UTF-8, legacy config/Save, AutoSave preserved, no relocation\n";
}

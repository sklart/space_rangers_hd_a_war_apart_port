#include "user_root.hpp"
#include "ec_file_adapter.hpp"
#include "pending_game_load.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    namespace user = srhd_awa::platform::user_root;
    assert(argc == 2);
    const auto root = fs::absolute(argv[1]);
    assert(!fs::exists(root));
    fs::create_directories(root / "config/Save");
    user::SetRoot(root.generic_string());
    const auto path = root / "config/Save" / fs::path(u"Ручной Ёж.sav");
    std::ofstream(path) << "preserved-user-slot";
    EC_HsFile::TPackCollectionEC collection{};
    EC_HsFile::TPackCollectionEC_Create(&collection);
    auto* loose = pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create);
    loose->UseLooseFiles = true;
    collection.AddPackToBack(loose);
    assert(collection.OpenAllPackages());
    pas::CriticalSection lock;
    lock.p_create();
    EC_HsFile::PackageCollection = &collection;
    EC_HsFile::PackageFileLock = &lock;
    EC_File::TFileEC file{};
    EC_File::TFileEC_Create(&file);
    const auto request = (root / "config/save" / fs::path(u"Ручной Ёж.sav")).u16string();
    srhd_awa::platform::pending_game_load::Store(request.c_str(), "?????? ??.sav");
    const auto forwarded = srhd_awa::platform::pending_game_load::Take("?????? ??.sav");
    assert(forwarded == request);
    assert(srhd_awa::platform::pending_game_load::Take("?????? ??.sav").empty());
    file.SetFileName(pas::WideString(forwarded.c_str()));
    assert(file.TryAcquireReadHandle(false));
    char bytes[19]{};
    file.ReadBuffer(bytes, 19);
    assert(std::string(bytes, 19) == "preserved-user-slot");
    file.ReleaseHandle();
    const auto next = (root / "config/save" / fs::path(u"Новая запись.sav")).u16string();
    file.SetFileName(pas::WideString(next.c_str()));
    file.CreateNew();
    char payload[] = "new manual slot";
    file.WriteBuffer(payload, sizeof(payload));
    file.ReleaseHandle();
    assert(user::ConfigFileExists(next.c_str()));
    file.SetFileName(pas::WideString(next.c_str()));
    file.AcquireReadHandle(false);
    char actual[sizeof(payload)]{};
    file.ReadBuffer(actual, sizeof(actual));
    assert(std::string(actual) == payload);
    file.ReleaseHandle();
    EC_File::TFileEC_Destroy(&file);
    collection.Clear(true);
    EC_HsFile::PackageCollection = nullptr;
    EC_HsFile::PackageFileLock = nullptr;
    srhd_awa::platform::pending_game_load::Store(request.c_str(), "manual.sav");
    assert(srhd_awa::platform::pending_game_load::Take("QuickSave.sav").empty());
    assert(srhd_awa::platform::pending_game_load::Take("manual.sav").empty());
    std::puts("PASS actual EC_File UTF-16 -> adapter -> disk -> reopen, legacy Save preserved");
}

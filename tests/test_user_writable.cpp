#include "ec_file_adapter.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {
void TearDownCollection() {
  if (EC_HsFile::PackageCollection) {
    EC_HsFile::PackageCollection->Clear(true);
    pas::free(EC_HsFile::PackageCollection);
    EC_HsFile::PackageCollection = nullptr;
  }
  if (EC_HsFile::PackageFileLock) {
    pas::free(EC_HsFile::PackageFileLock);
    EC_HsFile::PackageFileLock = nullptr;
  }
}

bool InitializeCollection() {
  EC_HsFile::PackageFileLock = pas::make_critical_section<pas::CriticalSection>();
  EC_HsFile::PackageCollection = pas::construct_call<EC_HsFile::TPackCollectionEC>(EC_HsFile::TPackCollectionEC_Create);
  auto* loose = pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create);
  loose->UseLooseFiles = true;
  EC_HsFile::PackageCollection->AddPackToFront(loose);
  return EC_HsFile::PackageCollection->OpenAllPackages();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::filesystem::path root(argv[1]);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  srhd_awa::platform::ec_file::SetUserRoot(root.string());
  if (!InitializeCollection()) return 1;

  const std::string config_path = (root / "config" / "CFG.TXT").generic_string();
  EC_File::TFileEC writer{};
  EC_File::TFileEC_Create(&writer);
  writer.SetFileName(pas::WideString(config_path.c_str()));
  const std::string payload = "CurrentVersion=2.1.2500\nVSync=True\n";
  bool ok = true;
  try {
    writer.CreateNew();
    writer.WriteBuffer(const_cast<char*>(payload.data()), static_cast<std::uint32_t>(payload.size()));
    ok = writer.GetSize() == payload.size() && writer.SetPointer(0, 0) == 0;
    writer.ReleaseHandle();
  } catch (...) {
    ok = false;
  }
  EC_File::TFileEC_Destroy(&writer);

  EC_File::TFileEC reader{};
  EC_File::TFileEC_Create(&reader);
  reader.SetFileName(pas::WideString(config_path.c_str()));
  std::string loaded(payload.size(), '\0');
  try {
    ok = ok && reader.TryAcquireReadHandle(false) && reader.GetSize() == payload.size();
    reader.ReadBuffer(loaded.data(), static_cast<std::uint32_t>(loaded.size()));
    reader.ReleaseHandle();
  } catch (...) {
    ok = false;
  }
  EC_File::TFileEC_Destroy(&reader);
  const bool config_ok = ok && loaded == payload;

  auto round_trip = [&](const std::filesystem::path& path,
                        const std::string& contents) {
    EC_File::TFileEC writer{};
    EC_File::TFileEC_Create(&writer);
    writer.SetFileName(pas::WideString(path.generic_string().c_str()));
    bool passed = false;
    bool write_ok = false;
    bool read_ok = false;
    try {
      writer.CreateNew();
      writer.WriteBuffer(const_cast<char*>(contents.data()),
                         static_cast<std::uint32_t>(contents.size()));
      writer.ReleaseHandle();
      write_ok = true;
      passed = std::filesystem::is_regular_file(path);
    } catch (...) {
      passed = false;
    }
    EC_File::TFileEC_Destroy(&writer);
    EC_File::TFileEC reader{};
    EC_File::TFileEC_Create(&reader);
    reader.SetFileName(pas::WideString(path.generic_string().c_str()));
    try {
      passed = passed && reader.TryAcquireReadHandle(false) &&
               reader.GetSize() == contents.size();
      read_ok = passed;
      std::string actual(contents.size(), '\0');
      if (passed) reader.ReadBuffer(actual.data(), static_cast<std::uint32_t>(actual.size()));
      reader.ReleaseHandle();
      passed = passed && actual == contents;
    } catch (...) {
      passed = false;
    }
    EC_File::TFileEC_Destroy(&reader);
    if (!passed)
      std::fprintf(stderr, "roundtrip failed path=%s write=%d exists=%d read=%d\n",
                   path.generic_string().c_str(), write_ok,
                   std::filesystem::is_regular_file(path), read_ok);
    return passed;
  };
  const bool newgame_ok = round_trip(root / "config" / "newgame.txt", "Race=0\nName=Tester\n");
  const bool save_ok = round_trip(root / "config" / "save" / "save.tmp", "temporary-save");
  ok = ok && newgame_ok && save_ok;
  ok = ok && !std::filesystem::exists(root / "newgame.txt") &&
       !std::filesystem::exists(root / "save" / "save.tmp");

  EC_File::TFileEC missing{};
  EC_File::TFileEC_Create(&missing);
  missing.SetFileName(pas::WideString((root / "config" / "save" / "missing.sav").generic_string().c_str()));
  try {
    ok = ok && !missing.TryAcquireReadHandle(false);
    missing.ReleaseHandle();
  } catch (...) {
    ok = false;
  }
  EC_File::TFileEC_Destroy(&missing);
  if (!ok || loaded != payload)
    std::fprintf(stderr, "user file checks config=%d newgame=%d save=%d missing=%d\n",
                 config_ok, newgame_ok, save_ok, ok);

  EC_File::TFileEC forbidden{};
  EC_File::TFileEC_Create(&forbidden);
  forbidden.SetFileName(u"../game.pkg");
  try {
    forbidden.CreateNew();
    ok = false;
  } catch (...) {
  }
  EC_File::TFileEC_Destroy(&forbidden);

  TearDownCollection();
  if (ok && loaded == payload) std::filesystem::remove_all(root, error);
  if (ok && loaded == payload) std::puts("portable writable user-file regression passed");
  return ok && loaded == payload ? 0 : 1;
}

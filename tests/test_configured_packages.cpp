#include "ec_file_adapter.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <cstdio>
#include <vector>

namespace {
EC_BlockPar::TBlockParEC* LoadConfig(const char16_t* name) {
  auto* config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  config->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(name), false);
  return config;
}

void FreeConfigState() {
  pas::free(GR_Main::InstallConfig);
  pas::free(GR_Main::LanguageInstallConfig);
  pas::free(GR_Main::ModInstallConfigs);
  pas::free(GR_Main::ModLanguageInstallConfigs);
  GR_Main::InstallConfig = nullptr;
  GR_Main::LanguageInstallConfig = nullptr;
  GR_Main::ModInstallConfigs = nullptr;
  GR_Main::ModLanguageInstallConfigs = nullptr;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  srhd_awa::platform::ec_file::SetGameRoot(argv[1]);
  if (!aPacket::InitializePackageCollection()) return 1;
  GR_Main::InstallConfig = LoadConfig(u"INSTALL.TXT");
  GR_Main::LanguageInstallConfig = LoadConfig(u"INSTALL_RUSSIAN.TXT");
  GR_Main::ModInstallConfigs = pas::make_object<pas::List>();
  GR_Main::ModLanguageInstallConfigs = pas::make_object<pas::List>();

  bool ok = aPacket::LoadConfiguredPackages();
  auto* collection = EC_HsFile::PackageCollection;
  ok = ok && collection->GetPackByIndex(0)->UseLooseFiles &&
       collection->GetPackByIndex(1)->PackagePath == "data\\russian.pkg" &&
       collection->GetPackByIndex(3)->PackagePath == "data\\voicesRus.pkg" &&
       collection->GetPackByIndex(17)->PackagePath == "data\\WSE.pkg";
  EC_File::TFileEC file{};
  EC_File::TFileEC_Create(&file);
  file.SetFileName(u"DATA/Asteroid/00.gai");
  ok = ok && file.TryAcquireReadHandle(false) && file.GetSize() > 0;
  if (file.OpenDepth != 0) file.ReleaseHandle();
  EC_File::TFileEC_Destroy(&file);

  FreeConfigState();
  aPacket::FinalizePackageCollection();
  if (ok) std::puts("configured package regression passed");
  return ok ? 0 : 1;
}

#include "ec_file_adapter.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <cstdio>
#include <filesystem>
#include <vector>

namespace {
void FreeConfigs() {
  for (std::int32_t index = 0; index < pas::list_count(GR_Main::ModInstallConfigs); ++index)
    pas::free(pas::list_at<pas::Object>(GR_Main::ModInstallConfigs, index));
  for (std::int32_t index = 0; index < pas::list_count(GR_Main::ModLanguageInstallConfigs); ++index)
    pas::free(pas::list_at<pas::Object>(GR_Main::ModLanguageInstallConfigs, index));
  pas::free(GR_Main::ModInstallConfigs); pas::free(GR_Main::ModLanguageInstallConfigs);
  pas::free(GR_Main::LanguageInstallConfig); pas::free(GR_Main::InstallConfig);
  GR_Main::ModInstallConfigs = nullptr; GR_Main::ModLanguageInstallConfigs = nullptr;
  GR_Main::LanguageInstallConfig = nullptr; GR_Main::InstallConfig = nullptr;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::filesystem::path root(argv[1]);
  srhd_awa::platform::ec_file::SetGameRoot(root.string());
  if (!aPacket::InitializePackageCollection()) return 1;
  GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
  pas::text_assign(GR_Main::SessionLog, (root / "m6-host-session.log").string().c_str(), false);
  GR_Main::SelectedLanguage = u"russian"_w;
  GR_Main::RequestedLanguage = pas::WideString();
  GR_Main::SkipModsOnReload = false;
  GR_Main::LoadLanguageAndPackages();

  auto* collection = EC_HsFile::PackageCollection;
  std::int32_t count = 0;
  while (collection->GetPackByIndex(count)) ++count;
  if (count != 18) {
    std::fprintf(stderr, "baseline mismatch: expected 18 package sources, got %d\n", count);
    FreeConfigs(); aPacket::FinalizePackageCollection(); return 1;
  }
  EC_File::TFileEC file{};
  EC_File::TFileEC_Create(&file);
  file.SetFileName(u"DATA/Asteroid/00.gai");
  const bool ok = file.TryAcquireReadHandle(false) && file.GetSize() == 246863;
  if (file.OpenDepth != 0) file.ReleaseHandle();
  EC_File::TFileEC_Destroy(&file);
  FreeConfigs(); aPacket::FinalizePackageCollection();
  if (GR_Main::SessionLogLock) { pas::free(GR_Main::SessionLogLock); GR_Main::SessionLogLock = nullptr; }
  if (ok) std::puts("real GR_Main release configuration regression passed");
  return ok ? 0 : 1;
}

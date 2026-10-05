#include "ec_file_adapter.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
bool WriteEmptyPackage(const std::filesystem::path& path) {
  if (FILE* file = std::fopen(path.string().c_str(), "wb")) {
    const std::uint32_t words[] = {4, 12, 0, 158};
    const bool ok = std::fwrite(words, sizeof(words), 1, file) == 1;
    std::fclose(file);
    return ok;
  }
  return false;
}

void FreeConfigs() {
  pas::free(GR_Main::ModInstallConfigs); pas::free(GR_Main::ModLanguageInstallConfigs);
  pas::free(GR_Main::LanguageInstallConfig); pas::free(GR_Main::InstallConfig);
  GR_Main::ModInstallConfigs = nullptr; GR_Main::ModLanguageInstallConfigs = nullptr;
  GR_Main::LanguageInstallConfig = nullptr; GR_Main::InstallConfig = nullptr;
}
}  // namespace

int main() {
  const std::filesystem::path root = "build/milestone6_no_mod";
  std::filesystem::create_directories(root / "DATA");
  std::ofstream(root / "INSTALL.TXT") << "Packages {\nPackage=DATA\\base.pkg\n}\n";
  std::ofstream(root / "INSTALL_RUSSIAN.TXT") << "Packages {\nPackage=DATA\\language.pkg\n}\n";
  if (!WriteEmptyPackage(root / "DATA" / "base.pkg") || !WriteEmptyPackage(root / "DATA" / "language.pkg")) return 1;

  srhd_awa::platform::ec_file::SetGameRoot(root.string());
  if (!aPacket::InitializePackageCollection()) return 1;
  GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
  GR_Main::SelectedLanguage = u"russian"_w;
  GR_Main::RequestedLanguage = pas::WideString();
  GR_Main::SkipModsOnReload = false;
  GR_Main::LoadLanguageAndPackages();

  auto* packages = EC_HsFile::PackageCollection;
  const bool ok = GR_Main::SelectedMods == u"" &&
      pas::list_count(GR_Main::ModInstallConfigs) == 0 && pas::list_count(GR_Main::ModLanguageInstallConfigs) == 0 &&
      packages->GetPackByIndex(0)->UseLooseFiles &&
      packages->GetPackByIndex(1)->PackagePath == "DATA\\language.pkg" &&
      packages->GetPackByIndex(2)->PackagePath == "DATA\\base.pkg" && !packages->GetPackByIndex(3);
  if (!ok) {
    std::fprintf(stderr, "no-mod startup mismatch: mods=%d language-mods=%d pack1=%s pack2=%s pack3=%p\n",
                 pas::list_count(GR_Main::ModInstallConfigs),
                 pas::list_count(GR_Main::ModLanguageInstallConfigs),
                 packages->GetPackByIndex(1) ? packages->GetPackByIndex(1)->PackagePath.c_str() : "<null>",
                 packages->GetPackByIndex(2) ? packages->GetPackByIndex(2)->PackagePath.c_str() : "<null>",
                 static_cast<void*>(packages->GetPackByIndex(3)));
  }
  aPacket::FinalizePackageCollection();
  FreeConfigs();
  std::filesystem::remove_all(root);
  return ok ? 0 : 1;
}

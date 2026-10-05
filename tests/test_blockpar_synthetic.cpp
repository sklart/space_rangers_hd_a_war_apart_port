#include "ec_file_adapter.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <cstdio>
#include <filesystem>

int main() {
  const std::filesystem::path root = "build/milestone5_loose";
  std::filesystem::create_directories(root / "DATA");
  if (FILE* file = std::fopen((root / "install.txt").string().c_str(), "wb")) {
    std::fputs("Packages {\nPackage=DATA\\A.pkg\nPackage=DATA\\B.pkg\n}\n", file);
    std::fclose(file);
  } else return 1;
  const auto write_empty_package = [](const std::filesystem::path& path) {
    if (FILE* package = std::fopen(path.string().c_str(), "wb")) {
      const std::uint32_t words[] = {4, 12, 0, 158};
      const bool ok = std::fwrite(words, sizeof(words), 1, package) == 1;
      std::fclose(package);
      return ok;
    }
    return false;
  };
  if (!write_empty_package(root / "DATA" / "base.pkg") ||
      !write_empty_package(root / "DATA" / "language.pkg") ||
      !write_empty_package(root / "DATA" / "mod.pkg") ||
      !write_empty_package(root / "DATA" / "mod_language.pkg")) return 1;
  srhd_awa::platform::ec_file::SetGameRoot(root.string());
  if (!aPacket::InitializePackageCollection()) return 1;
  auto* probe = pas::construct_call<EC_File::TFileEC>(EC_File::TFileEC_Create);
  probe->SetFileName(u"install.txt");
  probe->AcquireReadHandle(false);
  const bool probe_ok = probe->GetSize() > 0;
  probe->ReleaseHandle();
  pas::free(probe);
  if (!probe_ok) return 1;
  auto* config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  config->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
  auto* packages = config->GetBlock(u"Packages"sv);
  bool ok = packages->GetParamCount() == 2 &&
                  packages->GetParamValue(0) == pas::WideString(u"DATA\\A.pkg") &&
                  packages->GetParamValue(1) == pas::WideString(u"DATA\\B.pkg");
  pas::free(config);
  if (FILE* file = std::fopen((root / "base.txt").string().c_str(), "wb")) {
    std::fputs("Packages {\nPackage=DATA\\base.pkg\n}\n", file);
    std::fclose(file);
  } else return 1;
  if (FILE* file = std::fopen((root / "language.txt").string().c_str(), "wb")) {
    std::fputs("Packages {\nPackage=DATA\\language.pkg\n}\n", file);
    std::fclose(file);
  } else return 1;
  if (FILE* file = std::fopen((root / "mod.txt").string().c_str(), "wb")) {
    std::fputs("Packages {\nPackage=DATA\\mod.pkg\n}\n", file);
    std::fclose(file);
  } else return 1;
  if (FILE* file = std::fopen((root / "mod_language.txt").string().c_str(), "wb")) {
    std::fputs("Packages {\nPackage=DATA\\mod_language.pkg\n}\n", file);
    std::fclose(file);
  } else return 1;
  GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"base.txt"), false);
  GR_Main::LanguageInstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::LanguageInstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"language.txt"), false);
  GR_Main::ModInstallConfigs = pas::make_object<pas::List>();
  GR_Main::ModLanguageInstallConfigs = pas::make_object<pas::List>();
  auto* mod_config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  mod_config->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"mod.txt"), false);
  auto* mod_language_config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  mod_language_config->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"mod_language.txt"), false);
  pas::list_add(GR_Main::ModInstallConfigs, mod_config);
  pas::list_add(GR_Main::ModLanguageInstallConfigs, mod_language_config);
  ok = ok && aPacket::LoadConfiguredPackages() &&
       EC_HsFile::PackageCollection->GetPackByIndex(0)->UseLooseFiles &&
       EC_HsFile::PackageCollection->GetPackByIndex(1)->PackagePath == "DATA\\mod_language.pkg" &&
       EC_HsFile::PackageCollection->GetPackByIndex(2)->PackagePath == "DATA\\language.pkg" &&
       EC_HsFile::PackageCollection->GetPackByIndex(3)->PackagePath == "DATA\\mod.pkg" &&
       EC_HsFile::PackageCollection->GetPackByIndex(4)->PackagePath == "DATA\\base.pkg";
  pas::free(GR_Main::InstallConfig);
  pas::free(GR_Main::LanguageInstallConfig);
  pas::free(mod_config);
  pas::free(mod_language_config);
  pas::free(GR_Main::ModInstallConfigs);
  pas::free(GR_Main::ModLanguageInstallConfigs);
  GR_Main::InstallConfig = nullptr;
  GR_Main::LanguageInstallConfig = nullptr;
  GR_Main::ModInstallConfigs = nullptr;
  GR_Main::ModLanguageInstallConfigs = nullptr;
  aPacket::FinalizePackageCollection();
  std::filesystem::remove_all(root);
  return ok ? 0 : 1;
}

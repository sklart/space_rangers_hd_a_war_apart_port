#include "ec_file_adapter.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/SysUtilsImports.hpp"
#include "units/aPacket.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
void U32(FILE* file, std::uint32_t value) {
  for (int index = 0; index < 4; ++index) std::fputc((value >> (index * 8)) & 255, file);
}

bool WritePackage(const std::filesystem::path& path, const char* payload) {
  FILE* file = std::fopen(path.string().c_str(), "wb");
  if (!file) return false;
  const std::uint32_t size = static_cast<std::uint32_t>(std::strlen(payload));
  U32(file, 4); U32(file, 170); U32(file, 1); U32(file, 158);
  U32(file, 0); U32(file, size);
  char names[126]{};
  std::strncpy(names, "MARKER.BIN", 62);
  std::strncpy(names + 63, "marker.bin", 62);
  std::fwrite(names, 1, sizeof(names), file);
  U32(file, 0); U32(file, 0); U32(file, 0); U32(file, 0); U32(file, 174); U32(file, 0);
  U32(file, 0); std::fwrite(payload, 1, size, file);
  std::fclose(file);
  return true;
}

bool ReadMarker(const char* expected) {
  EC_File::TFileEC file{};
  EC_File::TFileEC_Create(&file);
  file.SetFileName(u"marker.bin");
  if (!file.TryAcquireReadHandle(false)) return false;
  std::vector<char> data(std::strlen(expected));
  file.ReadBuffer(data.data(), static_cast<std::uint32_t>(data.size()));
  file.ReleaseHandle();
  EC_File::TFileEC_Destroy(&file);
  return std::memcmp(data.data(), expected, data.size()) == 0;
}

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

int main() {
  const std::filesystem::path root = "build/milestone6_root";
  std::filesystem::create_directories(root / "DATA");
  std::filesystem::create_directories(root / "Mods" / "TestMod");
  std::ofstream(root / "INSTALL.TXT") << "Packages {\nPackage=DATA\\base.pkg\n}\n";
  std::ofstream(root / "INSTALL_RUSSIAN.TXT") << "Packages {\nPackage=DATA\\language.pkg\n}\n";
  std::ofstream(root / "Mods" / "ModCFG.txt") << "CurrentMod=TestMod\n";
  std::ofstream(root / "Mods" / "TestMod" / "Install.txt") << "Packages {\nPackage=DATA\\mod.pkg\n}\n";
  std::ofstream(root / "Mods" / "TestMod" / "Install_russian.txt") << "Packages {\nPackage=DATA\\mod_language.pkg\n}\n";
  if (!WritePackage(root / "DATA" / "base.pkg", "base") ||
      !WritePackage(root / "DATA" / "language.pkg", "language") ||
      !WritePackage(root / "DATA" / "mod.pkg", "mod") ||
      !WritePackage(root / "DATA" / "mod_language.pkg", "modlang")) return 1;
  std::ofstream(root / "marker.bin") << "loose";

  srhd_awa::platform::ec_file::SetGameRoot(root.string());
  const bool paths = SysUtilsImports::FileExists("INSTALL.TXT") &&
      SysUtilsImports::FileExists("install.txt") &&
      SysUtilsImports::FileExists("INSTALL_RUSSIAN.TXT") &&
      SysUtilsImports::FileExists("Mods\\ModCFG.txt") && SysUtilsImports::FileExists("Mods/ModCFG.txt");
  if (!paths || !aPacket::InitializePackageCollection()) return 1;
  GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
  pas::text_assign(GR_Main::SessionLog, (root / "session.log").string().c_str(), false);
  GR_Main::SelectedLanguage = u"russian"_w;
  GR_Main::RequestedLanguage = pas::WideString();
  GR_Main::SkipModsOnReload = false;
  GR_Main::LoadLanguageAndPackages();

  auto* packages = EC_HsFile::PackageCollection;
  bool ok = GR_Main::SelectedLanguage == u"russian" && GR_Main::SelectedMods == u"TestMod" &&
      pas::list_count(GR_Main::ModInstallConfigs) == 1 && pas::list_count(GR_Main::ModLanguageInstallConfigs) == 1 &&
      packages->GetPackByIndex(0)->UseLooseFiles &&
      packages->GetPackByIndex(1)->PackagePath == "DATA\\mod_language.pkg" &&
      packages->GetPackByIndex(2)->PackagePath == "DATA\\language.pkg" &&
      packages->GetPackByIndex(3)->PackagePath == "DATA\\mod.pkg" &&
      packages->GetPackByIndex(4)->PackagePath == "DATA\\base.pkg" && ReadMarker("loose");
  std::filesystem::remove(root / "marker.bin");
  ok = ok && ReadMarker("modlang");
  aPacket::FinalizePackageCollection();
  FreeConfigs();
  if (GR_Main::SessionLogLock) { pas::free(GR_Main::SessionLogLock); GR_Main::SessionLogLock = nullptr; }
  std::filesystem::remove_all(root);
  return ok ? 0 : 1;
}

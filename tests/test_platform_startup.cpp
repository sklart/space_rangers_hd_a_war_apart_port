#include "startup_slice.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/MessageText.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
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
  std::strncpy(names, "MARKER.BIN", 62); std::strncpy(names + 63, "marker.bin", 62);
  std::fwrite(names, 1, sizeof(names), file);
  U32(file, 0); U32(file, 0); U32(file, 0); U32(file, 0); U32(file, 174); U32(file, 0);
  U32(file, 0); std::fwrite(payload, 1, size, file); std::fclose(file);
  return true;
}

bool ReadMarker(const char* expected) {
  EC_File::TFileEC file{};
  EC_File::TFileEC_Create(&file);
  file.SetFileName(u"marker.bin");
  if (!file.TryAcquireReadHandle(false)) return false;
  std::vector<char> bytes(std::strlen(expected));
  file.ReadBuffer(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
  file.ReleaseHandle(); EC_File::TFileEC_Destroy(&file);
  return std::memcmp(bytes.data(), expected, bytes.size()) == 0;
}

bool Clean() {
  return EC_HsFile::PackageCollection == nullptr && GR_Main::InstallConfig == nullptr &&
      GR_Main::LanguageInstallConfig == nullptr && GR_Main::MainWindowHandle == 0 &&
      MessageText::QuestMessages == nullptr;
}

void WriteConfigs(const std::filesystem::path& root, bool language, bool invalid_package) {
  std::filesystem::create_directories(root / "DATA");
  std::ofstream(root / "INSTALL.TXT") << "Packages {\nPackage=DATA\\"
      << (invalid_package ? "invalid.pkg" : "base.pkg") << "\n}\n";
  if (language) std::ofstream(root / "INSTALL_RUSSIAN.TXT") << "Packages {\nPackage=DATA\\language.pkg\n}\n";
}
}  // namespace

int main() {
  srhd_awa::platform::runtime_platform::State uninitialized_platform;
  std::string window_error;
  bool ok = !srhd_awa::platform::runtime_platform::CreateMainWindow(&uninitialized_platform, &window_error) &&
      uninitialized_platform.window_token == 0 && !uninitialized_platform.services_initialized;
  const std::filesystem::path root = "build/milestone7_startup";
  WriteConfigs(root, true, false);
  if (!WritePackage(root / "DATA" / "base.pkg", "base") || !WritePackage(root / "DATA" / "language.pkg", "language")) return 1;

  srhd_awa::platform::startup_slice::State state;
  std::string error;
  ok = ok && srhd_awa::platform::startup_slice::Initialize(&state, root.string(), (root / "user").string(), (root / "startup.log").string(), &error);
  ok = ok && state.platform.window_token != 0 && GR_Main::MainWindowHandle == state.platform.window_token &&
      GR_Main::PerformanceCounterFrequency > 0 && MessageText::QuestMessages != nullptr &&
      EC_HsFile::PackageCollection->GetPackByIndex(0)->UseLooseFiles && ReadMarker("language");
  srhd_awa::platform::startup_slice::Shutdown(&state);
  ok = ok && Clean() && !state.platform.services_initialized;
  error.clear();
  ok = ok && srhd_awa::platform::startup_slice::Initialize(&state, root.string(), (root / "user").string(), (root / "startup.log").string(), &error) &&
      state.platform.window_token != 0 && ReadMarker("language");
  srhd_awa::platform::startup_slice::Shutdown(&state);
  ok = ok && Clean();

  const std::filesystem::path no_install = "build/milestone7_missing_install";
  std::filesystem::create_directories(no_install);
  error.clear();
  ok = ok && !srhd_awa::platform::startup_slice::Initialize(&state, no_install.string(), (no_install / "user").string(), (no_install / "startup.log").string(), &error) && Clean();
  const std::filesystem::path no_language = "build/milestone7_missing_language";
  WriteConfigs(no_language, false, false);
  WritePackage(no_language / "DATA" / "base.pkg", "base");
  error.clear();
  ok = ok && !srhd_awa::platform::startup_slice::Initialize(&state, no_language.string(), (no_language / "user").string(), (no_language / "startup.log").string(), &error) && Clean();
  const std::filesystem::path invalid = "build/milestone7_invalid_package";
  WriteConfigs(invalid, true, true);
  WritePackage(invalid / "DATA" / "language.pkg", "language");
  std::ofstream(invalid / "DATA" / "invalid.pkg") << "not a package";
  error.clear();
  ok = ok && !srhd_awa::platform::startup_slice::Initialize(&state, invalid.string(), (invalid / "user").string(), (invalid / "startup.log").string(), &error) && Clean();

  std::filesystem::remove_all(root); std::filesystem::remove_all(no_install);
  std::filesystem::remove_all(no_language); std::filesystem::remove_all(invalid);
  return ok ? 0 : 1;
}

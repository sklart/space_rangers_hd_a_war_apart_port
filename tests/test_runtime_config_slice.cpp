#include "runtime_settings_slice.hpp"
#include "startup_slice.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const std::filesystem::path game_root(argv[1]);
  const std::filesystem::path user_root(argv[2]);
  std::error_code error;
  std::filesystem::remove_all(user_root, error);

  srhd_awa::platform::startup_slice::State state;
  std::string startup_error;
  if (!srhd_awa::platform::startup_slice::Initialize(
          &state, game_root.string(), user_root.string(), (user_root / "startup.log").string(),
          &startup_error)) {
    std::fprintf(stderr, "startup failed: %s\n", startup_error.c_str());
    return 1;
  }

  std::string runtime_error;
  bool ok = srhd_awa::platform::runtime_settings_slice::Initialize(&runtime_error);
  ok = ok && GR_Main::CCInterface != nullptr && GR_Main::MainDataConfig != nullptr &&
       GR_Main::LanguageDataConfig != nullptr && GR_Main::CacheDataRoot != nullptr &&
       GR_Main::GameDataConfig != nullptr && GR_Main::UiStyleConfig != nullptr &&
       GR_Main::UiDepthConfig != nullptr && GR_Main::WideCaseTable.length() > 0;
  ok = ok && GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get()) != nullptr &&
       GR_Main::MainDataConfig->GetBlockByPath(u"ML"_wref.get()) != nullptr &&
       GR_Main::MainDataConfig->GetBlockByPath(u"ZPos"_wref.get()) != nullptr &&
       GR_Main::LanguageDataConfig->GetBlockByPath(u"CaseConv"_wref.get()) != nullptr &&
       GR_Main::LanguageDataConfig->GetBlockByPath(u"PlanetQuest"_wref.get()) != nullptr;
  ok = ok && std::filesystem::is_regular_file(user_root / "config" / "CFG.TXT");

  srhd_awa::platform::runtime_settings_slice::Shutdown();
  ok = ok && GR_Main::CCInterface == nullptr && GR_Main::MainDataConfig == nullptr &&
       GR_Main::LanguageDataConfig == nullptr && GR_Main::CacheDataRoot == nullptr &&
       GR_Main::GameDataConfig == nullptr && GR_Main::UserSettingsConfig == nullptr &&
       GR_Main::WideCaseTable.length() == 0;
  srhd_awa::platform::startup_slice::Shutdown(&state);
  std::filesystem::remove_all(user_root, error);
  if (!ok) std::fprintf(stderr, "runtime config slice failed: %s\n", runtime_error.c_str());
  if (ok) std::puts("real DAT/runtime configuration regression passed");
  return ok ? 0 : 1;
}

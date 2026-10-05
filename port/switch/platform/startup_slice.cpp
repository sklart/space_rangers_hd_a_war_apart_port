#include "startup_slice.hpp"

#include "ec_file_adapter.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/MessageText.hpp"
#include "units/aPacket.hpp"

namespace srhd_awa::platform::startup_slice {
namespace {
void FreeConfigState() {
  if (GR_Main::ModInstallConfigs) {
    for (std::int32_t index = 0; index < pas::list_count(GR_Main::ModInstallConfigs); ++index)
      pas::free(pas::list_at<pas::Object>(GR_Main::ModInstallConfigs, index));
    pas::free(GR_Main::ModInstallConfigs);
  }
  if (GR_Main::ModLanguageInstallConfigs) {
    for (std::int32_t index = 0; index < pas::list_count(GR_Main::ModLanguageInstallConfigs); ++index)
      pas::free(pas::list_at<pas::Object>(GR_Main::ModLanguageInstallConfigs, index));
    pas::free(GR_Main::ModLanguageInstallConfigs);
  }
  pas::free(GR_Main::LanguageInstallConfig);
  pas::free(GR_Main::InstallConfig);
  GR_Main::ModInstallConfigs = nullptr;
  GR_Main::ModLanguageInstallConfigs = nullptr;
  GR_Main::LanguageInstallConfig = nullptr;
  GR_Main::InstallConfig = nullptr;
}
}  // namespace

bool Initialize(State* state, const std::string& game_root, const std::string& user_root,
                const std::string& gr_main_log_path,
                std::string* error) {
  if (!state || state->package_collection_initialized || state->platform.services_initialized ||
      GR_Main::InstallConfig || GR_Main::LanguageInstallConfig) {
    if (error) *error = "startup state is already initialized";
    return false;
  }
  srhd_awa::platform::ec_file::SetGameRoot(game_root);
  srhd_awa::platform::ec_file::SetUserRoot(user_root);
  const char* stage = "platform services";
  try {
    if (!runtime_platform::InitializePlatformServices(&state->platform, error)) return false;
    stage = "package collection";
    GR_Main::PerformanceCounterFrequency = state->platform.timing_frequency;
    if (!aPacket::InitializePackageCollection()) {
      if (error) *error = "package collection initialization failed";
      Shutdown(state);
      return false;
    }
    state->package_collection_initialized = true;
    stage = "main window";
    if (!runtime_platform::CreateMainWindow(&state->platform, error)) {
      Shutdown(state);
      return false;
    }
    // A token is deliberately used instead of truncating an SDL_Window pointer.
    GR_Main::MainWindowHandle = state->platform.window_token;
    stage = "install config";
    GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
    GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
    stage = "startup log";
    pas::text_assign(GR_Main::SessionLog, gr_main_log_path.c_str(), false);
    // Portable equivalent of CreateStartupLogFile: use the caller's writable root,
    // retain the release file-create/write/close sequence, and avoid Documents/VCL.
    pas::text_open(GR_Main::SessionLog, 3, false);
    pas::text_writeln(GR_Main::SessionLog, "Start"_a, false);
    pas::text_close(GR_Main::SessionLog, false);
    GR_Main::SelectedLanguage = u"russian"_w;
    GR_Main::RequestedLanguage = pas::WideString();
    GR_Main::SkipModsOnReload = false;
    stage = "language and packages";
    GR_Main::LoadLanguageAndPackages();
    stage = "quest messages";
    MessageText::QuestMessages = pas::construct_call<MessageText::TQuestMessages>(MessageText::TQuestMessages_Create);
    return true;
  } catch (...) {
    if (error) *error = std::string("translated startup configuration failed at ") + stage;
    Shutdown(state);
    return false;
  }
}

void Shutdown(State* state) {
  if (!state) return;
  if (MessageText::QuestMessages) {
    pas::free(MessageText::QuestMessages);
    MessageText::QuestMessages = nullptr;
  }
  FreeConfigState();
  if (state->package_collection_initialized) aPacket::FinalizePackageCollection();
  state->package_collection_initialized = false;
  GR_Main::MainWindowHandle = 0;
  GR_Main::PerformanceCounterFrequency = 0;
  GR_Main::SelectedLanguage = pas::WideString();
  GR_Main::RequestedLanguage = pas::WideString();
  GR_Main::SelectedMods = pas::WideString();
  if (GR_Main::SessionLogLock) {
    pas::free(GR_Main::SessionLogLock);
    GR_Main::SessionLogLock = nullptr;
  }
  runtime_platform::ShutdownPlatformServices(&state->platform);
}
}  // namespace srhd_awa::platform::startup_slice

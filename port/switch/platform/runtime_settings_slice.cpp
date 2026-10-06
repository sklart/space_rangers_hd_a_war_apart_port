#include "runtime_settings_slice.hpp"

#include "units/EC_BlockPar.hpp"
#include "units/EC_Cache.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_Str.hpp"
#include "units/GI_Main.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"
#include "units/SysUtilsImports.hpp"
#include "units/aMyFunction.hpp"

namespace srhd_awa::platform::runtime_settings_slice {
namespace {
bool HasParam(EC_BlockPar::TBlockParEC* block, const char16_t* name) {
  return block && block->CountParams(name) > 0;
}

bool ReadEnabled(EC_BlockPar::TBlockParEC* block, const char16_t* name, bool fallback) {
  if (!HasParam(block, name)) return fallback;
  return GI_Main::ParseEnabledNameGI(pas::view(EC_Str::TrimWideString(block->GetParamByPathOrMarker(name)))) != 0;
}

void FreeOwnedConfigs() {
  pas::free(GR_Main::NewGameSettingsConfig);
  GR_Main::NewGameSettingsConfig = nullptr;
  pas::free(GR_Main::UserSettingsConfig);
  GR_Main::UserSettingsConfig = nullptr;
  GR_Main::UiStyleConfig = nullptr;
  GR_Main::UiDepthConfig = nullptr;
  GR_Main::GameDataConfig = nullptr;
  GR_Main::WideCaseTable = nullptr;
}

void InitializeGlobalCache() {
  constexpr std::int32_t kDefaultCacheMiB = 256;
  constexpr std::int32_t kMaxCacheMiB = 2047;
  std::int32_t cache_mib = kDefaultCacheMiB;
  if (HasParam(GR_Main::UserSettingsConfig, u"CacheSize")) {
    const auto configured = EC_Str::ExtractDigitsToIntW(GR_Main::UserSettingsConfig->GetParam(u"CacheSize"sv));
    if (configured > 0 && configured <= kMaxCacheMiB) cache_mib = configured;
  }
  GR_Main::GlobalCache = pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
  GR_Main::GlobalCache->SetDataRoot(GR_Main::CacheDataRoot);
  GR_Main::GlobalCache->ResidentByteLimit = cache_mib * 1024 * 1024;
}
void BuildDerivedRuntimeState() {
  auto* main_data = GR_Main::MainDataConfig;
  auto* language_data = GR_Main::LanguageDataConfig;
  if (!main_data || !language_data || !GR_Main::CacheDataRoot) {
    pas::raise(pas::make_exception<pas::Exception>("portable DAT roots are unavailable"_a));
  }
  GR_Main::UiStyleConfig = main_data->GetBlockByPath(u"ML"_wref.get());
  GR_Main::GameDataConfig = main_data->GetBlockByPath(u"Data"_wref.get());
  GR_Main::UiDepthConfig = main_data->GetBlockByPath(u"ZPos"_wref.get());
  GR_Main::CacheDataRoot->AddMissingFromBlock(language_data->GetBlockByPath(u"PlanetQuest"_wref.get()));
  GR_Main::LoadInformationColorTags();

  GlobalsV::PlanetDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"Planet"sv));
  GlobalsV::ShipPathDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"UnitPathShip"sv));
  GlobalsV::ShipPathEndDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"UnitPathEndShip"sv));
  GlobalsV::UnitPathDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"UnitPath"sv));
  GlobalsV::UnitPathEndDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"UnitPathEnd"sv));
  GlobalsV::ActionButtonDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"ButtonAction"sv));
  GlobalsV::GalaxyStarDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"GalaxyStar"sv));
  GlobalsV::GalaxyStarNameDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"GalaxyStarName"sv));
  GlobalsV::GalaxyWarDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"GalaxyWar"sv));
  GlobalsV::ConstellationLineDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"ConstellationLine"sv));
  GlobalsV::ConstellationColorDepth = EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u"ConstellationColor"sv));

  auto* case_conv = language_data->GetBlock(u"CaseConv"sv);
  const auto count = case_conv->GetParamCount();
  GR_Main::WideCaseTable.set_length(count);
  for (std::int32_t index = 0; index < count; ++index) {
    GR_Main::WideCaseTable[index].LowerChar = case_conv->GetParamName(index).read(1);
    GR_Main::WideCaseTable[index].UpperChar = case_conv->GetParamValue(index).read(1);
  }
}

void LoadUserSettings() {
  const pas::WideString path = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"CFG.TXT"});
  GR_Main::UserSettingsConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  const bool exists = SysUtilsImports::FileExists(static_cast<pas::AnsiString>(path));
  if (exists) {
    GR_Main::UserSettingsConfig->LoadFromTextFileWithEncodingProbe(path.pchar(), true);
  } else {
    GR_Main::UserSettingsConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"cfg.txt"), false);
    // Materialize the shipped template in the portable writable root before
    // applying migrations, matching the release copy-then-open lifecycle.
    GR_Main::UserSettingsConfig->SaveTextFile(path.pchar(), true, false);
  }
  bool changed = !exists;
  if (!HasParam(GR_Main::UserSettingsConfig, u"CurrentVersion")) {
    GR_Main::UserSettingsConfig->AddParam(u"CurrentVersion"_wref.get(), GR_Main::GameVersionText);
    GR_Main::UserSettingsConfig->AddParam(u"HardwareRender"_wref.get(), u"True"_wref.get());
    GR_Main::UserSettingsConfig->AddParam(u"MultiThread"_wref.get(), u"False"_wref.get());
    changed = true;
  } else {
    const auto current = GR_Main::UserSettingsConfig->GetParam(u"CurrentVersion"sv);
    if (current == u"2.1.1800" && HasParam(GR_Main::UserSettingsConfig, u"CountFilmSave") &&
        GR_Main::UserSettingsConfig->GetParam(u"CountFilmSave"sv) == u"30") {
      GR_Main::UserSettingsConfig->SetParam(u"CountFilmSave"sv, u"7"_wref.get());
      changed = true;
    }
    if (current != GR_Main::GameVersionText) {
      GR_Main::UserSettingsConfig->SetParam(u"CurrentVersion"sv, GR_Main::GameVersionText);
      changed = true;
    }
  }
  if (!HasParam(GR_Main::UserSettingsConfig, u"VideoMemSizeLimit")) {
    GR_Main::UserSettingsConfig->AddParam(u"VideoMemSizeLimit"_wref.get(), u"256"_wref.get());
    changed = true;
  }
  if (changed) GR_Main::UserSettingsConfig->SaveTextFile(path.pchar(), true, false);

  GR_Main::VSyncEnabled = ReadEnabled(GR_Main::UserSettingsConfig, u"VSync", false);
  GlobalsV::ScaleViewportToWindow = ReadEnabled(GR_Main::UserSettingsConfig, u"RenderModeScale", true);
  GR_Main::PresentWithoutLimit = ReadEnabled(GR_Main::UserSettingsConfig, u"DisableFrameLimit", false);
  GlobalsV::HardwareRenderingRequested = ReadEnabled(GR_Main::UserSettingsConfig, u"HardwareRender", false);
  GlobalsV::HardwareRenderingEnabled = false;

  GR_Main::NewGameSettingsConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  const pas::WideString new_game_path = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"newgame.txt"});
  if (SysUtilsImports::FileExists(static_cast<pas::AnsiString>(new_game_path))) {
    GR_Main::NewGameSettingsConfig->LoadFromTextFileWithEncodingProbe(new_game_path.pchar(), true);
  }
}
}  // namespace

bool Initialize(std::string* error, BeforeDerivedRuntimeStateHook before_derived, void* hook_context) {
  if (GR_Main::MainDataConfig || GR_Main::LanguageDataConfig || GR_Main::CacheDataRoot ||
      GR_Main::UserSettingsConfig || GR_Main::CCInterface || GR_Main::GlobalCache) {
    if (error) *error = "runtime settings slice is already initialized";
    return false;
  }
  try {
    GR_Main::CCInterface = pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create);
    GR_Main::LoadDatConfigAndModOverrides();
    LoadUserSettings();
    if (before_derived) before_derived(hook_context);
    BuildDerivedRuntimeState();
    InitializeGlobalCache();
    return true;
  } catch (...) {
    if (error) *error = "portable DAT/runtime configuration failed";
    Shutdown();
    return false;
  }
}

void Shutdown() {
  pas::free(GR_Main::GlobalCache);
  GR_Main::GlobalCache = nullptr;
  FreeOwnedConfigs();
  GR_Main::FreeDatConfigRoots();
  pas::free(GR_Main::CCInterface);
  GR_Main::CCInterface = nullptr;
}

}  // namespace srhd_awa::platform::runtime_settings_slice

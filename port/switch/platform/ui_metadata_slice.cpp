#include "ui_metadata_slice.hpp"

#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"
#include "units/EC_Str.hpp"
#include "units/GI_Main.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"

namespace srhd_awa::platform::ui_metadata_slice {
namespace {
std::vector<FontResolution> g_font_resolutions;
bool g_initialized = false;

bool HasParam(EC_BlockPar::TBlockParEC* block, const char16_t* name) {
  return block && block->CountParams(name) > 0;
}

std::string Narrow(const pas::WideString& text) {
  return static_cast<pas::AnsiString>(text).c_str();
}

void SetReleaseFontNames() {
  GlobalsV::RangerFontName = u"Font.2Ranger"_w;
  GlobalsV::MiniFontName = u"Font.2Mini"_w;
  GlobalsV::SmallFontName = u"Font.2Small"_w;
  GlobalsV::SmallBoldFontName = u"Font.2SmallBold"_w;
  GlobalsV::NormalFontName = u"Font.2Normal"_w;
  GlobalsV::NormalBoldFontName = u"Font.2NormalBold"_w;
  GlobalsV::BigFontName = u"Font.2Big"_w;
  GlobalsV::HugeFontName = u"Font.2Huge"_w;
  GlobalsV::IntroFontName = u"Font.2Intro"_w;
  GlobalsV::AuthorsFontName = u"Font.2Authors"_w;
  GlobalsV::SmoothSmallFontName = u"Font.Verdana8"_w;
  GlobalsV::SmoothSmallBoldFontName = u"Font.Verdana8bold"_w;
  GlobalsV::SmoothNormalFontName = u"Font.Verdana9"_w;
  GlobalsV::SmoothNormalBoldFontName = u"Font.Verdana9bold"_w;
  GlobalsV::SmoothBigFontName = u"Font.Verdana11"_w;
  GlobalsV::SmoothHugeFontName = u"Font.Verdana12"_w;
  GlobalsV::SmoothIntroFontName = u"Font.Verdana13"_w;
}

void RecordResolution(const pas::WideString& key) {
  FontResolution result{.key = Narrow(key)};
  auto* entry = GR_Main::CacheDataRoot->FindEntryByPath(key);
  result.found = entry != nullptr;
  if (entry) result.kind = static_cast<std::uint32_t>(entry->Kind);
  if (entry && entry->Kind == EC_Data::dekFile) {
    result.file_exists = GR_Main::CacheDataRoot->FileExistsByPath(key) != 0;
    if (entry->SharedFileRef && entry->SharedFileRef->FileRef)
      result.filename = Narrow(entry->SharedFileRef->FileRef->FileName);
  }
  g_font_resolutions.push_back(std::move(result));
}

void ClearOwnedState() {
  GlobalsV::RangerFontName = pas::WideString();
  GlobalsV::MiniFontName = pas::WideString();
  GlobalsV::SmallFontName = pas::WideString();
  GlobalsV::SmallBoldFontName = pas::WideString();
  GlobalsV::NormalFontName = pas::WideString();
  GlobalsV::NormalBoldFontName = pas::WideString();
  GlobalsV::BigFontName = pas::WideString();
  GlobalsV::HugeFontName = pas::WideString();
  GlobalsV::IntroFontName = pas::WideString();
  GlobalsV::AuthorsFontName = pas::WideString();
  GlobalsV::SmoothSmallFontName = pas::WideString();
  GlobalsV::SmoothSmallBoldFontName = pas::WideString();
  GlobalsV::SmoothNormalFontName = pas::WideString();
  GlobalsV::SmoothNormalBoldFontName = pas::WideString();
  GlobalsV::SmoothBigFontName = pas::WideString();
  GlobalsV::SmoothHugeFontName = pas::WideString();
  GlobalsV::SmoothIntroFontName = pas::WideString();
  GlobalsV::FontSmoothingEnabled = false;
  g_font_resolutions.clear();
}
}  // namespace

bool Initialize(std::string* error) {
  if (g_initialized) {
    if (error) *error = "UI metadata slice is already initialized";
    return false;
  }
  if (!GR_Main::CacheDataRoot || !GR_Main::GlobalCache || !GR_Main::UserSettingsConfig) {
    if (error) *error = "M13 requires CacheDataRoot, GlobalCache and UserSettingsConfig";
    return false;
  }
  try {
    SetReleaseFontNames();
    GlobalsV::FontSmoothingEnabled = HasParam(GR_Main::UserSettingsConfig, u"FontSmooth") &&
        GI_Main::ParseEnabledNameGI(pas::view(EC_Str::TrimWideString(
            GR_Main::UserSettingsConfig->GetParamByPathOrMarker(u"FontSmooth")))) != 0;
    const pas::WideString keys[] = {
        GlobalsV::RangerFontName, GlobalsV::MiniFontName, GlobalsV::SmallFontName,
        GlobalsV::SmallBoldFontName, GlobalsV::NormalFontName, GlobalsV::NormalBoldFontName,
        GlobalsV::BigFontName, GlobalsV::HugeFontName, GlobalsV::IntroFontName,
        GlobalsV::AuthorsFontName, GlobalsV::SmoothSmallFontName, GlobalsV::SmoothSmallBoldFontName,
        GlobalsV::SmoothNormalFontName, GlobalsV::SmoothNormalBoldFontName, GlobalsV::SmoothBigFontName,
        GlobalsV::SmoothHugeFontName, GlobalsV::SmoothIntroFontName,
    };
    for (const auto& key : keys) RecordResolution(key);
    g_initialized = true;
    return true;
  } catch (...) {
    ClearOwnedState();
    if (error) *error = "M13 font metadata initialization failed";
    return false;
  }
}

void Shutdown() {
  if (!g_initialized) return;
  ClearOwnedState();
  g_initialized = false;
}

const std::vector<FontResolution>& FontResolutions() { return g_font_resolutions; }

}  // namespace srhd_awa::platform::ui_metadata_slice
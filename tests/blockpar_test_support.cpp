#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/GR_Main.hpp"
#include "units/SysUtilsImports.hpp"
#include "units/aPacket.hpp"
#include "zlib_bridge.hpp"

// M5/M6 host checks exercise package configuration and BlockPar parsing. They
// deliberately do not link GR_Main.cpp: that unit retains dormant UI, audio,
// cache and registry references which are outside the portable slice.
namespace GR_Main {
EC_BlockPar::TBlockParEC* InstallConfig{};
EC_BlockPar::TBlockParEC* LanguageInstallConfig{};
pas::List* ModInstallConfigs{};
pas::List* ModLanguageInstallConfigs{};
TCCInterface* CCInterface{};
pas::DynArray<EC_Str::TWideCasePair> WideCaseTable{};
pas::WideString SelectedLanguage{};
pas::WideString RequestedLanguage{};
pas::WideString SelectedMods{};
pas::WideString SelectedModsDisplaySuffix{};
std::uint8_t SkipModsOnReload{};
pas::CriticalSection* SessionLogLock{};
pas::TextFile SessionLog{};
const pas::WideString ModSelectionConfigPath = u"Mods\\ModCFG.txt"_w;
std::int64_t PerformanceCounterFrequency{};
std::uint32_t MainWindowHandle{};

void AppendLogLineThreadSafe(const pas::AnsiString&) {}
void AppendLogTextThreadSafe(const pas::AnsiString&) {}
void LogMemoryUsage() {}
std::int32_t PAS_STDCALL OKGF_ZLib_Compress(void* destination, void* source,
                                             std::int32_t source_size, std::int32_t mode) {
  return srhd_awa::platform::zlib_bridge::Compress(destination, source, source_size, mode);
}
std::int32_t PAS_STDCALL OKGF_ZLib_UnCompress(void* destination, std::int32_t destination_capacity,
                                               void* source, std::int32_t source_size) {
  return srhd_awa::platform::zlib_bridge::Uncompress(destination, destination_capacity, source, source_size);
}
void TCCInterface::SetResourceChecksumFailed(std::uint8_t) {}

void LoadSelectedModInstallBlocks() {
  pas::WideString mod_names{};
  if (SysUtilsImports::FileExists(static_cast<pas::AnsiString>(ModSelectionConfigPath))) {
    auto* selection = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
    selection->LoadFromTextFileWithEncodingProbe(ModSelectionConfigPath.pchar(), false);
    if (selection->CountParams(u"CurrentMod"_wref.get()) > 0)
      mod_names = EC_Str::TrimWideString(selection->GetParam(u"CurrentMod"sv));
    pas::free(selection);
  }
  SelectedMods = mod_names;
  SelectedModsDisplaySuffix = mod_names == u"" ? pas::WideString() : pas::concat_wide({u", ", mod_names, u","});
  if (SkipModsOnReload) return;
  for (std::int32_t index = 0; index < EC_Str::CountDelimitedPartsW(pas::view(mod_names), u","sv); ++index) {
    auto path = EC_Str::TrimWideString(EC_Str::ExtractDelimitedPartW(pas::view(mod_names), index, u","sv));
    if (path != u"") path = pas::concat_wide({path, u"\\"});
    const auto install = pas::concat_wide({u"Mods\\", path, u"Install.txt"});
    if (SysUtilsImports::FileExists(static_cast<pas::AnsiString>(install))) {
      auto* block = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
      block->LoadFromTextFileWithEncodingProbe(install.pchar(), false);
      pas::list_add(ModInstallConfigs, block);
    }
    const auto language_install = pas::concat_wide({u"Mods\\", path, u"Install_", SelectedLanguage, u".txt"});
    if (SysUtilsImports::FileExists(static_cast<pas::AnsiString>(language_install))) {
      auto* block = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
      block->LoadFromTextFileWithEncodingProbe(language_install.pchar(), false);
      pas::list_add(ModLanguageInstallConfigs, block);
    }
  }
}

void LoadLanguageAndPackages() {
  if (RequestedLanguage != u"" && SysUtilsImports::FileExists(static_cast<pas::AnsiString>(pas::concat_wide({u"install_", RequestedLanguage, u".txt"}))))
    SelectedLanguage = RequestedLanguage;
  if (SelectedLanguage == u"" || !SysUtilsImports::FileExists(static_cast<pas::AnsiString>(pas::concat_wide({u"install_", SelectedLanguage, u".txt"}))))
    SelectedLanguage = u"russian"_w;
  const auto language_install = pas::concat_wide({u"install_", SelectedLanguage, u".txt"});
  if (!SysUtilsImports::FileExists(static_cast<pas::AnsiString>(language_install)))
    pas::raise(pas::make_exception<pas::Exception>("not installed language"_a));
  LanguageInstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  LanguageInstallConfig->LoadFromTextFileWithEncodingProbe(language_install.pchar(), false);
  ModInstallConfigs = pas::make_object<pas::List>();
  ModLanguageInstallConfigs = pas::make_object<pas::List>();
  LoadSelectedModInstallBlocks();
  if (!aPacket::LoadConfiguredPackages())
    pas::raise(pas::make_exception<pas::Exception>("configured packages failed"_a));
}
}  // namespace GR_Main

namespace System {
std::uint32_t RandSeed{};
}  // namespace System

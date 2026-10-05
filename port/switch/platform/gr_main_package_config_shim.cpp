#include "units/GR_Main.hpp"
#include "units/GR_DX.hpp"
#include "units/SysUtils.hpp"
#include "units/SysUtilsImports.hpp"
#include "units/System.hpp"
#include "units/WindowsSdk.hpp"

namespace GR_Main {
EC_BlockPar::TBlockParEC* InstallConfig{};
EC_BlockPar::TBlockParEC* LanguageInstallConfig{};
pas::List* ModInstallConfigs{};
pas::List* ModLanguageInstallConfigs{};
pas::DynArray<EC_Str::TWideCasePair> WideCaseTable{};
TCCInterface* CCInterface{};

void TCCInterface::SetResourceChecksumFailed(std::uint8_t) {}

void AppendLogLineThreadSafe(const pas::AnsiString&) {}
void AppendLogTextThreadSafe(const pas::AnsiString&) {}
void LogMemoryUsage() {}

std::int32_t PAS_STDCALL OKGF_ZLib_Compress(void*, void*, std::int32_t, std::int32_t) {
  return -1;
}

std::int32_t PAS_STDCALL OKGF_ZLib_UnCompress(void*, std::int32_t, void*, std::int32_t) {
  return -1;
}
}  // namespace GR_Main

namespace GR_DX {
void EvictTextureCaches(std::uint8_t) {}
}  // namespace GR_DX

namespace SysUtils {
std::uint8_t DecimalSeparator{'.'};
}  // namespace SysUtils

namespace SysUtilsImports {
void PAS_STDCALL Sleep(std::uint32_t) {}
}  // namespace SysUtilsImports

#if !defined(__SWITCH__)
namespace System {
std::uint32_t RandSeed{};
}  // namespace System
#endif

namespace WindowsSdk {
std::int32_t PAS_STDCALL RegCloseKey(HKEY) { return 1; }

std::int32_t PAS_STDCALL RegCreateKeyExW(HKEY, char16_t*, std::uint32_t, char16_t*,
                                         std::uint32_t, REGSAM, PSecurityAttributes,
                                         HKEY&, PDWORD) {
  return 1;
}

std::int32_t PAS_STDCALL RegSetValueExW(HKEY, char16_t*, std::uint32_t,
                                        std::uint32_t, void*, std::uint32_t) {
  return 1;
}
}  // namespace WindowsSdk

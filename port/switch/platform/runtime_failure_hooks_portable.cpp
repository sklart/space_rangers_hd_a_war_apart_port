#include "units/GR_DX.hpp"
#include "units/GR_Main.hpp"
#include "units/SysUtils.hpp"
#include "units/System.hpp"
#include "units/WindowsSdk.hpp"

namespace GR_DX {
std::uint32_t ResidentTextureBytes{};
void EvictTextureCaches(std::uint8_t) {}
}  // namespace GR_DX

namespace SysUtils {
std::uint8_t DecimalSeparator{'.'};
}  // namespace SysUtils

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

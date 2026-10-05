#include "units/GR_DX.hpp"
#include "units/GR_Main.hpp"
#include "units/MMSystem.hpp"
#include "units/GlobalsV.hpp"
#include "units/SysUtils.hpp"
#include "units/System.hpp"
#include "units/WindowsSdk.hpp"
#include "units/WindowsImports.hpp"

namespace GR_DX {
std::uint8_t TextureManagerDisabled{};
WindowsSdk::TPoint MaxTextureSize{};
GR_DX::TScreenVerticesGR DrawVertices{};
pas::DynArray<GR_DX::TScreenVertexGR> PendingPoints{};
pas::List* TextureCaches{};
std::uint32_t AvailableTextureBytes{};
GR_DX::TCircleTableGR CircleCos{};
GR_DX::TCircleTableGR CircleSin{};
GR_DX::TLineAlphaTableGR LineAlphaTable{};
std::uint32_t ReservedTextureBytes{};
std::int32_t TextureIdleSeconds{120};
std::uint32_t LastTextureEvictionTick{};
std::int32_t PendingPointCount{};
std::int32_t PendingPointCapacity{};
std::uint32_t ResidentTextureBytes{};
void EvictTextureCaches(std::uint8_t) {}
void GR_CreateTexture(std::int32_t, std::int32_t, std::uint32_t, std::uint32_t,
                      Direct3D9::IDirect3DTexture9& result) {
  result = nullptr;
}
}  // namespace GR_DX

namespace MMSystem {
std::uint32_t PAS_STDCALL timeGetTime() { return WindowsImports::GetTickCount(); }
}  // namespace MMSystem

namespace GlobalsV {
std::uint8_t HardwareRenderingRequested{};
std::uint8_t HardwareRenderingEnabled{};
std::uint8_t ScaleViewportToWindow{true};
}  // namespace GlobalsV

namespace SysUtils {
std::uint8_t DecimalSeparator{'.'};
}  // namespace SysUtils

#if !defined(__SWITCH__) && !defined(SRHD_PORTABLE_RENDERER)
namespace System {
std::uint32_t RandSeed{};
}  // namespace System
#endif

#if !defined(__SWITCH__)
namespace WindowsImports {
std::uint32_t PAS_STDCALL GetTickCount() { return 0; }
}  // namespace WindowsImports
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

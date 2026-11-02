// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace srhd_awa::matrixgame {
// Original Rangers Win32 storage, not the host SDK ABI. Keep original globals
// and records unchanged; translate at the actual SDK call boundary instead.
#pragma pack(push,1)
struct RangersLegacyPresentation {
 std::uint32_t BackBufferWidth,BackBufferHeight,BackBufferFormat,BackBufferCount;
 std::uint32_t MultiSampleType,MultiSampleQuality,SwapEffect,DeviceWindow;
 std::int32_t Windowed,EnableAutoDepthStencil;
 std::uint32_t AutoDepthStencilFormat,Flags,FullScreenRefreshRateInHz,PresentationInterval;
};
#pragma pack(pop)
static_assert(sizeof(RangersLegacyPresentation)==56);
static_assert(offsetof(RangersLegacyPresentation,DeviceWindow)==28);
static_assert(offsetof(RangersLegacyPresentation,Windowed)==32);
// Windows reference only. Performs the real IDirect3DDevice9::Reset, preserves
// its HRESULT and copies SDK in/out fields back without crossing 56 bytes.
// Device is borrowed; the caller must release all reset-sensitive resources.
std::int32_t ResetRangersLegacyPresentation(void* device,void* parameters);
// A port window token is never a pointer. Bind its real borrowed Windows SDK
// identity explicitly when a portable Rangers caller owns the SDL window.
class RangersLegacyWindowBinding {
public:
 RangersLegacyWindowBinding(std::uint32_t token,void* native_window);
 ~RangersLegacyWindowBinding();
 RangersLegacyWindowBinding(const RangersLegacyWindowBinding&)=delete;
 RangersLegacyWindowBinding& operator=(const RangersLegacyWindowBinding&)=delete;
private:std::uint32_t token_;void* native_window_;
 friend std::int32_t ResetRangersLegacyPresentation(void*,void*);
 static thread_local RangersLegacyWindowBinding* active_;
};
}

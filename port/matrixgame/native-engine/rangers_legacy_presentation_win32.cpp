// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "rangers_legacy_presentation.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace srhd_awa::matrixgame {
thread_local RangersLegacyWindowBinding* RangersLegacyWindowBinding::active_{};
RangersLegacyWindowBinding::RangersLegacyWindowBinding(std::uint32_t token,void* window):token_(token),native_window_(window){
 if(active_)throw std::logic_error("Rangers legacy window identity is already bound");
 if(!token || !IsWindow(static_cast<HWND>(window)))throw std::invalid_argument("Rangers legacy window requires a live SDK window and nonzero token");
 active_=this;
}
RangersLegacyWindowBinding::~RangersLegacyWindowBinding(){if(active_!=this)std::terminate();active_=nullptr;}
std::int32_t ResetRangersLegacyPresentation(void* device,void* parameters){
 if(!device || !parameters)return D3DERR_INVALIDCALL;
 RangersLegacyPresentation old{};std::memcpy(&old,parameters,sizeof(old));
 D3DPRESENT_PARAMETERS sdk{};
 sdk.BackBufferWidth=old.BackBufferWidth;sdk.BackBufferHeight=old.BackBufferHeight;
 sdk.BackBufferFormat=static_cast<D3DFORMAT>(old.BackBufferFormat);sdk.BackBufferCount=old.BackBufferCount;
 sdk.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(old.MultiSampleType);sdk.MultiSampleQuality=old.MultiSampleQuality;
 sdk.SwapEffect=static_cast<D3DSWAPEFFECT>(old.SwapEffect);
 auto* binding=RangersLegacyWindowBinding::active_;
 if(binding && old.DeviceWindow!=binding->token_)return D3DERR_INVALIDCALL;
 sdk.hDeviceWindow=binding?static_cast<HWND>(binding->native_window_):reinterpret_cast<HWND>(static_cast<std::uintptr_t>(old.DeviceWindow));
 sdk.Windowed=old.Windowed;sdk.EnableAutoDepthStencil=old.EnableAutoDepthStencil;
 sdk.AutoDepthStencilFormat=static_cast<D3DFORMAT>(old.AutoDepthStencilFormat);sdk.Flags=old.Flags;
 sdk.FullScreen_RefreshRateInHz=old.FullScreenRefreshRateInHz;sdk.PresentationInterval=old.PresentationInterval;
 const HRESULT result=static_cast<IDirect3DDevice9*>(device)->Reset(&sdk);
 const auto window=reinterpret_cast<std::uintptr_t>(sdk.hDeviceWindow);
 if(binding){if(sdk.hDeviceWindow!=binding->native_window_)throw std::runtime_error("SDK Reset replaced the registered window identity");}
 else if(window>std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("SDK Reset returned an unrepresentable legacy HWND");
 old.BackBufferWidth=sdk.BackBufferWidth;old.BackBufferHeight=sdk.BackBufferHeight;
 old.BackBufferFormat=sdk.BackBufferFormat;old.BackBufferCount=sdk.BackBufferCount;
 old.MultiSampleType=sdk.MultiSampleType;old.MultiSampleQuality=sdk.MultiSampleQuality;
 old.SwapEffect=sdk.SwapEffect;old.DeviceWindow=binding?binding->token_:static_cast<std::uint32_t>(window);
 old.Windowed=sdk.Windowed;old.EnableAutoDepthStencil=sdk.EnableAutoDepthStencil;
 old.AutoDepthStencilFormat=sdk.AutoDepthStencilFormat;old.Flags=sdk.Flags;
 old.FullScreenRefreshRateInHz=sdk.FullScreen_RefreshRateInHz;old.PresentationInterval=sdk.PresentationInterval;
 std::memcpy(parameters,&old,sizeof(old));return result;
}
}

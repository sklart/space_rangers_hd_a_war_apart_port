// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "device_state_backend.hpp"
namespace srhd_awa::matrixgame::platform3g {
// Real Windows SDK implementation, borrowed HWND / IDirect3DDevice9.
// Opaque pointers keep SDK types out of the portable engine contract.
class Win32Window final:public Window {
public:
    explicit Win32Window(void* hwnd):hwnd_(hwnd){}
    void* Identity()const noexcept override{return hwnd_;}
    std::unique_ptr<WindowAttachment> Attach(MessageHandler) override;
    void Show(Visibility) override;
    void ReleaseCursor() override;
    std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t) override;
    CursorHandle ExchangeCursor(CursorHandle)override;
    CursorHandle ReadCursor()override;
    void DestroyCursorIcon(IconHandle)override;
    void Update()override;
    WindowRect ReadClientRect()override;
    WindowRect ReadWindowRect()override;
    std::unique_ptr<TimerPeriodLease> AcquireTimerPeriod(std::uint32_t)override;
    std::size_t OutstandingTimerLeases()const noexcept{return timer_leases_;}
private:void* hwnd_;std::size_t timer_leases_{};
};
class D3D9Device final:public Device,public StoredStateBackend {
public:
    explicit D3D9Device(void* device):device_(device){}
    D3D9Device(const D3D9Device&)=delete;
    D3D9Device& operator=(const D3D9Device&)=delete;
    DeviceCaps ReadCaps() override;
    CreationParameters ReadCreationParameters()override;
    DisplayMode ReadAdapterDisplayMode(std::uint32_t)override;
    DeviceResult CheckMultisample(std::uint32_t,std::uint32_t,PixelFormat,bool,std::uint32_t)override;
    GammaRamp ReadGamma(std::uint32_t) override;
    void RestoreGamma(std::uint32_t,const GammaRamp&)noexcept override;
    std::uint64_t AvailableTextureBytes() override;
    DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t) override;
    std::uint32_t ReadSamplerState(std::uint32_t,SamplerRegister) override;
    std::unique_ptr<DeviceStateLease> CaptureState()override;
    std::size_t OutstandingStateLeases()const override{return leases_;}
    DeviceResult SetWorldTransform(const WorldMatrix&)override;
    WorldMatrix ReadWorldTransform()override;
    DeviceResult SetRenderState(RasterRegister,std::uint32_t)override;
    std::uint32_t ReadRenderState(RasterRegister)override;
    DeviceResult ClearTexture(std::uint32_t)override;
    bool TextureBound(std::uint32_t)override;
    DeviceResult SetTextureStageState(std::uint32_t,TextureStageRegister,std::uint32_t)override;
    std::uint32_t ReadTextureStageState(std::uint32_t,TextureStageRegister)override;
    DeviceResult GetRenderState(StoredRenderRegister,std::uint32_t*)override;
    DeviceResult SetRenderState(StoredRenderRegister,std::uint32_t)override;
    DeviceResult GetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t*)override;
    DeviceResult SetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t)override;
private:void* device_;std::size_t leases_{};
};
}

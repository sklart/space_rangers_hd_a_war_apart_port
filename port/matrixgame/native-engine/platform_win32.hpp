// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
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
private:void* hwnd_;
};
class D3D9Device final:public Device {
public:
    explicit D3D9Device(void* device):device_(device){}
    DeviceCaps ReadCaps() override;
    GammaRamp ReadGamma(std::uint32_t) override;
    void RestoreGamma(std::uint32_t,const GammaRamp&)noexcept override;
    std::uint64_t AvailableTextureBytes() override;
    DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t) override;
    std::uint32_t ReadSamplerState(std::uint32_t,SamplerRegister) override;
private:void* device_;
};
}

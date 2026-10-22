// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include <algorithm>
namespace srhd_awa::matrixgame::platform3g {
namespace {
constexpr wchar_t property[]=L"SRHD.MatrixGame.Native3G.Attachment";
struct Attachment final:WindowAttachment {
    HWND hwnd;WNDPROC previous{};Win32Window& window;MessageHandler handler;bool installed{};
    Attachment(HWND h,Win32Window& w,MessageHandler f):hwnd(h),window(w),handler(f) {
        if(!IsWindow(hwnd) || GetPropW(hwnd,property))throw std::logic_error("window invalid or already attached");
        previous=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
        if(!previous)throw std::runtime_error("GetWindowLongPtrW failed");
        if(!SetPropW(hwnd,property,this))throw std::runtime_error("SetPropW failed");
        SetLastError(0);
        const auto replaced=SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&Dispatch));
        if(!replaced && GetLastError()) {RemovePropW(hwnd,property);throw std::runtime_error("SetWindowLongPtrW failed");}
        installed=true;
    }
    ~Attachment(){Restore();}
    static LRESULT CALLBACK Dispatch(HWND h,UINT m,WPARAM w,LPARAM l) {
        auto* self=static_cast<Attachment*>(GetPropW(h,property));
        return self?self->handler(self->window,m,w,l):DefWindowProcW(h,m,w,l);
    }
    void Restore()noexcept override {
        if(!installed)return;
        // Do not overwrite a hook installed later by another owner.
        if(IsWindow(hwnd) && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC))==&Dispatch)
            SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(previous));
        if(GetPropW(hwnd,property)==this)RemovePropW(hwnd,property);
        installed=false;
    }
};
IDirect3DDevice9& DeviceRef(void* p) {
    if(!p)throw std::invalid_argument("null borrowed D3D9 device");
    return *static_cast<IDirect3DDevice9*>(p);
}
}
std::unique_ptr<WindowAttachment> Win32Window::Attach(MessageHandler handler) {
    if(!handler)throw std::invalid_argument("null window handler");
    return std::make_unique<Attachment>(static_cast<HWND>(hwnd_),*this,handler);
}
void Win32Window::Show(Visibility v) {
    ShowWindow(static_cast<HWND>(hwnd_),v==Visibility::Maximized?SW_MAXIMIZE:v==Visibility::Restored?SW_RESTORE:SW_MINIMIZE);
}
void Win32Window::ReleaseCursor(){ClipCursor(nullptr);}
std::intptr_t Win32Window::DefaultMessage(std::uint32_t m,std::uintptr_t w,std::intptr_t l) {
    return DefWindowProcW(static_cast<HWND>(hwnd_),m,w,l);
}
DeviceCaps D3D9Device::ReadCaps() {
    auto& device=DeviceRef(device_);D3DCAPS9 caps{};
    if(FAILED(device.GetDeviceCaps(&caps)))throw std::runtime_error("IDirect3DDevice9::GetDeviceCaps failed");
    return {bool(caps.Caps2&D3DCAPS2_CANCALIBRATEGAMMA),caps.MaxTextureBlendStages,caps.MaxAnisotropy,device.GetNumberOfSwapChains()};
}
GammaRamp D3D9Device::ReadGamma(std::uint32_t index) {
    auto& device=DeviceRef(device_);
    if(index>=device.GetNumberOfSwapChains())throw std::out_of_range("gamma swapchain");
    D3DGAMMARAMP ramp{};device.GetGammaRamp(index,&ramp);GammaRamp result;
    std::copy_n(ramp.red,256,result.red.begin());std::copy_n(ramp.green,256,result.green.begin());std::copy_n(ramp.blue,256,result.blue.begin());
    return result;
}
void D3D9Device::RestoreGamma(std::uint32_t index,const GammaRamp& saved)noexcept {
    auto& device=*static_cast<IDirect3DDevice9*>(device_);D3DGAMMARAMP ramp{};
    std::copy_n(saved.red.begin(),256,ramp.red);std::copy_n(saved.green.begin(),256,ramp.green);std::copy_n(saved.blue.begin(),256,ramp.blue);
    device.SetGammaRamp(index,0,&ramp);
}
std::uint64_t D3D9Device::AvailableTextureBytes(){return DeviceRef(device_).GetAvailableTextureMem();}
DeviceResult D3D9Device::SetSamplerState(std::uint32_t sampler,SamplerRegister type,std::uint32_t value) {
    return static_cast<DeviceResult>(DeviceRef(device_).SetSamplerState(sampler,static_cast<D3DSAMPLERSTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadSamplerState(std::uint32_t sampler,SamplerRegister type) {
    DWORD value{};
    if(FAILED(DeviceRef(device_).GetSamplerState(sampler,static_cast<D3DSAMPLERSTATETYPE>(type),&value)))
        throw std::runtime_error("IDirect3DDevice9::GetSamplerState failed");
    return value;
}
}

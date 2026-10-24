// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#endif
#include "offscreen_backend.hpp"
namespace srhd_awa::matrixgame::offscreen {
using namespace platform3g;
#ifdef _WIN32
namespace {
IDirect3DDevice9& ActualDevice() {
    auto* provider=dynamic_cast<D3D9Device*>(&ReadyDevice());
    if(!provider)throw Unsupported("offscreen operation requires actual D3D9 provider");
    const auto handles=provider->ReadLegacyHandles();
    if(!handles.device)throw Unsupported("offscreen SDK identity unavailable");
    return *reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
}
#endif
DeviceResult Readback(IDirect3DSurface9* source,IDirect3DSurface9* destination) {
#ifdef _WIN32
    return ActualDevice().GetRenderTargetData(source,destination);
#else
    (void)source;(void)destination;(void)ReadyDevice();throw Unsupported("offscreen GPU readback unavailable");
#endif
}
DeviceResult CreateTarget(std::uint32_t width,std::uint32_t height,std::uint32_t format,std::uint32_t multisample,
    std::uint32_t quality,bool lockable,IDirect3DSurface9** output,std::uintptr_t shared) {
#ifdef _WIN32
    return ActualDevice().CreateRenderTarget(width,height,static_cast<D3DFORMAT>(format),static_cast<D3DMULTISAMPLE_TYPE>(multisample),quality,lockable,output,reinterpret_cast<HANDLE*>(shared));
#else
    (void)width;(void)height;(void)format;(void)multisample;(void)quality;(void)lockable;(void)output;(void)shared;
    (void)ReadyDevice();throw Unsupported("offscreen render target unavailable");
#endif
}
DeviceResult CreateDepth(std::uint32_t width,std::uint32_t height,std::uint32_t format,std::uint32_t multisample,
    std::uint32_t quality,bool discard,IDirect3DSurface9** output,std::nullptr_t) {
#ifdef _WIN32
    return ActualDevice().CreateDepthStencilSurface(width,height,static_cast<D3DFORMAT>(format),static_cast<D3DMULTISAMPLE_TYPE>(multisample),quality,discard,output,nullptr);
#else
    (void)width;(void)height;(void)format;(void)multisample;(void)quality;(void)discard;(void)output;
    (void)ReadyDevice();throw Unsupported("offscreen depth target unavailable");
#endif
}
DeviceResult GetDepth(IDirect3DSurface9** output) {
#ifdef _WIN32
    return ActualDevice().GetDepthStencilSurface(output);
#else
    (void)output;(void)ReadyDevice();throw Unsupported("offscreen depth query unavailable");
#endif
}
DeviceResult SetDepth(IDirect3DSurface9* surface) {
#ifdef _WIN32
    return ActualDevice().SetDepthStencilSurface(surface);
#else
    (void)surface;(void)ReadyDevice();throw Unsupported("offscreen depth binding unavailable");
#endif
}
DeviceResult GetTarget(std::uint32_t index,IDirect3DSurface9** output) {
#ifdef _WIN32
    return ActualDevice().GetRenderTarget(index,output);
#else
    (void)index;(void)output;(void)ReadyDevice();throw Unsupported("offscreen target query unavailable");
#endif
}
DeviceResult SetTarget(std::uint32_t index,IDirect3DSurface9* surface) {
#ifdef _WIN32
    return ActualDevice().SetRenderTarget(index,surface);
#else
    (void)index;(void)surface;(void)ReadyDevice();throw Unsupported("offscreen target binding unavailable");
#endif
}
DeviceResult Describe(IDirect3DSurface9* surface,SurfaceDesc* output) {
    if(!surface || !output)throw std::invalid_argument("null offscreen description");
#ifdef _WIN32
    D3DSURFACE_DESC value{};const auto result=surface->GetDesc(&value);
    if(SUCCEEDED(result))*output={static_cast<std::uint32_t>(value.Format),static_cast<std::uint32_t>(value.MultiSampleType),value.MultiSampleQuality,value.Width,value.Height};
    return result;
#else
    throw Unsupported("offscreen surface description unavailable");
#endif
}
DeviceResult Lock(IDirect3DSurface9* surface,legacy_texture::LockedRect* output,std::nullptr_t,std::uint32_t flags) {
    if(!surface || !output)throw std::invalid_argument("null offscreen lock");
#ifdef _WIN32
    D3DLOCKED_RECT value{};const auto result=surface->LockRect(&value,nullptr,flags);
    if(SUCCEEDED(result))*output={value.Pitch,value.pBits};return result;
#else
    (void)flags;throw Unsupported("offscreen surface lock unavailable");
#endif
}
DeviceResult Unlock(IDirect3DSurface9* surface) {
    if(!surface)throw std::invalid_argument("null offscreen unlock");
#ifdef _WIN32
    return surface->UnlockRect();
#else
    throw Unsupported("offscreen surface unlock unavailable");
#endif
}
void Release(IDirect3DSurface9* surface)noexcept {
#ifdef _WIN32
    if(surface)surface->Release();
#else
    if(surface)std::terminate();
#endif
}
DeviceResult Clear(std::uint32_t count,const void* rectangles,std::uint32_t flags,std::uint32_t color,float depth,std::uint32_t stencil) {
    if(count || rectangles)throw std::invalid_argument("offscreen clear supports original full target call only");
#ifdef _WIN32
    return ActualDevice().Clear(0,nullptr,flags,color,depth,stencil);
#else
    (void)flags;(void)color;(void)depth;(void)stencil;(void)ReadyDevice();throw Unsupported("offscreen clear unavailable");
#endif
}
DeviceResult BeginScene() {
#ifdef _WIN32
    return ActualDevice().BeginScene();
#else
    (void)ReadyDevice();throw Unsupported("offscreen begin scene unavailable");
#endif
}
DeviceResult EndScene() {
#ifdef _WIN32
    return ActualDevice().EndScene();
#else
    (void)ReadyDevice();throw Unsupported("offscreen end scene unavailable");
#endif
}
DeviceResult GetViewport(VideoViewport* output) {
    if(!output)throw std::invalid_argument("null offscreen viewport query");
#ifdef _WIN32
    D3DVIEWPORT9 value{};const auto result=ActualDevice().GetViewport(&value);
    if(SUCCEEDED(result))*output={value.X,value.Y,value.Width,value.Height,value.MinZ,value.MaxZ};
    return result;
#else
    throw Unsupported("offscreen viewport query unavailable");
#endif
}
DeviceResult SetViewportValue(const VideoViewport& value) {
#ifdef _WIN32
    const D3DVIEWPORT9 viewport{value.X,value.Y,value.Width,value.Height,value.MinZ,value.MaxZ};
    return ActualDevice().SetViewport(&viewport);
#else
    (void)value;(void)ReadyDevice();throw Unsupported("offscreen viewport submission unavailable");
#endif
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#endif
#include "offscreen_backend.hpp"
#include "native_map_queries.hpp"
#ifndef _WIN32
#include "native_surfaces.hpp"
#include "native_texture.hpp"
#endif
#include "native_frame_scope.hpp"
#include <cstring>
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
    try{native_surfaces::Copy(source,destination,true);return 0;}catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
#endif
}
DeviceResult CreateTarget(std::uint32_t width,std::uint32_t height,std::uint32_t format,std::uint32_t multisample,
    std::uint32_t quality,bool lockable,IDirect3DSurface9** output,std::uintptr_t shared) {
#ifdef _WIN32
    return ActualDevice().CreateRenderTarget(width,height,static_cast<D3DFORMAT>(format),static_cast<D3DMULTISAMPLE_TYPE>(multisample),quality,lockable,output,reinterpret_cast<HANDLE*>(shared));
#else
    if(!output || shared)throw std::invalid_argument("native offscreen target output/shared");*output=nullptr;
    try{auto session=native_surfaces::Active();native_surfaces::Expose(session->Create({format,multisample,quality,width,height},false,lockable),output);return 0;}catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
#endif
}
DeviceResult CreateDepth(std::uint32_t width,std::uint32_t height,std::uint32_t format,std::uint32_t multisample,
    std::uint32_t quality,bool discard,IDirect3DSurface9** output,std::nullptr_t) {
#ifdef _WIN32
    return ActualDevice().CreateDepthStencilSurface(width,height,static_cast<D3DFORMAT>(format),static_cast<D3DMULTISAMPLE_TYPE>(multisample),quality,discard,output,nullptr);
#else
    if(!output)throw std::invalid_argument("native depth output");*output=nullptr;(void)discard;
    try{auto session=native_surfaces::Active();native_surfaces::Expose(session->Create({format,multisample,quality,width,height},true,false),output);return 0;}catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
#endif
}
DeviceResult GetDepth(IDirect3DSurface9** output) {
#ifdef _WIN32
    return ActualDevice().GetDepthStencilSurface(output);
#else
    if(!output)throw std::invalid_argument("native depth query output");*output=nullptr;auto session=native_surfaces::Active();if(!session->depth)return static_cast<std::int32_t>(0x88760866u);native_surfaces::Expose(session->depth,output);return 0;
#endif
}
DeviceResult SetDepth(IDirect3DSurface9* surface) {
#ifdef _WIN32
    return ActualDevice().SetDepthStencilSurface(surface);
#else
    native_surfaces::Active()->Depth(surface?native_surfaces::Find(surface):nullptr);return 0;
#endif
}
DeviceResult GetTarget(std::uint32_t index,IDirect3DSurface9** output) {
#ifdef _WIN32
    return ActualDevice().GetRenderTarget(index,output);
#else
    if(!output || index)throw std::invalid_argument("native target query index/output");*output=nullptr;auto session=native_surfaces::Active();if(!session->color)throw Unsupported("native presentation surface unavailable");native_surfaces::Expose(session->color,output);return 0;
#endif
}
DeviceResult SetTarget(std::uint32_t index,IDirect3DSurface9* surface) {
#ifdef _WIN32
    return ActualDevice().SetRenderTarget(index,surface);
#else
    if(index)throw std::invalid_argument("native target binding index");native_surfaces::Active()->Color(native_surfaces::Find(surface));return 0;
#endif
}
DeviceResult Describe(IDirect3DSurface9* surface,SurfaceDesc* output) {
    if(!surface || !output)throw std::invalid_argument("null offscreen description");
#ifdef _WIN32
    D3DSURFACE_DESC value{};const auto result=surface->GetDesc(&value);
    if(SUCCEEDED(result))*output={static_cast<std::uint32_t>(value.Format),static_cast<std::uint32_t>(value.MultiSampleType),value.MultiSampleQuality,value.Width,value.Height};
    return result;
#else
    auto value=native_surfaces::Find(surface);if(value->session.lock()!=native_surfaces::Active() || (!value->System() && !value->resource))throw Unsupported("native surface descriptor device expired");*output=value->desc;return 0;
#endif
}
DeviceResult Lock(IDirect3DSurface9* surface,legacy_texture::LockedRect* output,std::nullptr_t,std::uint32_t flags) {
    if(!surface || !output)throw std::invalid_argument("null offscreen lock");
#ifdef _WIN32
    D3DLOCKED_RECT value{};const auto result=surface->LockRect(&value,nullptr,flags);
    if(SUCCEEDED(result))*output={value.Pitch,value.pBits};return result;
#else
    return native_surfaces::Lock(surface,output,flags);
#endif
}
DeviceResult Unlock(IDirect3DSurface9* surface) {
    if(!surface)throw std::invalid_argument("null offscreen unlock");
#ifdef _WIN32
    return surface->UnlockRect();
#else
    return native_surfaces::Unlock(surface);
#endif
}
void Release(IDirect3DSurface9* surface)noexcept {
#ifdef _WIN32
    if(surface)surface->Release();
#else
    native_surfaces::Release(surface);
#endif
}
DeviceResult Clear(std::uint32_t count,const void* rectangles,std::uint32_t flags,std::uint32_t color,float depth,std::uint32_t stencil) {
#ifdef _WIN32
    if(count || rectangles)throw std::invalid_argument("offscreen clear supports original full target call only");
    return ActualDevice().Clear(0,nullptr,flags,color,depth,stencil);
#else
    if(count>4096 || bool(count)!=bool(rectangles))throw std::invalid_argument("native clear rectangle count/pointer");
    auto* provider=dynamic_cast<NativeFrameProvider*>(&ReadyDevice());if(!provider)throw Unsupported("native clear provider unavailable");
    std::vector<ClearRectangle> copied(count);
    if(count)std::memcpy(copied.data(),rectangles,std::size_t(count)*sizeof(ClearRectangle));
    return provider->NativeClear(copied,flags,color,depth,stencil);
#endif
}
DeviceResult BeginScene() {
#ifdef _WIN32
    return ActualDevice().BeginScene();
#else
    auto* provider=dynamic_cast<NativeFrameProvider*>(&ReadyDevice());if(!provider)throw Unsupported("native scene provider unavailable");
    return provider->NativeBeginScene();
#endif
}
DeviceResult EndScene() {
#ifdef _WIN32
    return ActualDevice().EndScene();
#else
    auto* provider=dynamic_cast<NativeFrameProvider*>(&ReadyDevice());if(!provider)throw Unsupported("native scene provider unavailable");
    return provider->NativeEndScene();
#endif
}
DeviceResult GetViewport(VideoViewport* output) {
    if(!output)throw std::invalid_argument("null offscreen viewport query");
#ifdef _WIN32
    D3DVIEWPORT9 value{};const auto result=ActualDevice().GetViewport(&value);
    if(SUCCEEDED(result))*output={value.X,value.Y,value.Width,value.Height,value.MinZ,value.MaxZ};
    return result;
#else
    auto* provider=dynamic_cast<NativeMapQueries*>(&ReadyDevice());
    if(!provider)throw Unsupported("offscreen native viewport provider unavailable");
    *output=provider->NativeReadViewport();return 0;
#endif
}
DeviceResult SetViewportValue(const VideoViewport& value) {
#ifdef _WIN32
    const D3DVIEWPORT9 viewport{value.X,value.Y,value.Width,value.Height,value.MinZ,value.MaxZ};
    return ActualDevice().SetViewport(&viewport);
#else
    return ReadyDevice().SetVideoViewport(value);
#endif
}
}

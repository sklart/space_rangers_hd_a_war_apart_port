// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "legacy_texture_backend.hpp"
struct IDirect3DSurface9;
namespace srhd_awa::matrixgame::offscreen {
struct SurfaceDesc {std::uint32_t Format{},MultiSampleType{},MultiSampleQuality{},Width{},Height{};};
platform3g::DeviceResult CreateTarget(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,bool,IDirect3DSurface9**,std::uintptr_t);
platform3g::DeviceResult CreateDepth(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,bool,IDirect3DSurface9**,std::nullptr_t);
platform3g::DeviceResult GetDepth(IDirect3DSurface9**);
platform3g::DeviceResult SetDepth(IDirect3DSurface9*);
platform3g::DeviceResult GetTarget(std::uint32_t,IDirect3DSurface9**);
platform3g::DeviceResult SetTarget(std::uint32_t,IDirect3DSurface9*);
platform3g::DeviceResult Describe(IDirect3DSurface9*,SurfaceDesc*);
platform3g::DeviceResult Lock(IDirect3DSurface9*,legacy_texture::LockedRect*,std::nullptr_t,std::uint32_t);
platform3g::DeviceResult Unlock(IDirect3DSurface9*);
void Release(IDirect3DSurface9*)noexcept;
platform3g::DeviceResult Clear(std::uint32_t,const void*,std::uint32_t,std::uint32_t,float,std::uint32_t);
platform3g::DeviceResult BeginScene();
platform3g::DeviceResult EndScene();
platform3g::DeviceResult GetViewport(platform3g::VideoViewport*);
platform3g::DeviceResult SetViewportValue(const platform3g::VideoViewport&);
template<class Viewport> platform3g::DeviceResult SetViewport(const Viewport* viewport) {
    if(!viewport)throw std::invalid_argument("null offscreen viewport");
    const platform3g::VideoViewport value{viewport->X,viewport->Y,viewport->Width,viewport->Height,viewport->MinZ,viewport->MaxZ};
    return SetViewportValue(value);
}
}

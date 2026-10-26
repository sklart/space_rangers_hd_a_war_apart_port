// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include "platform_win32.hpp"
#endif
#include "unmanaged_texture_backend.hpp"
#ifndef _WIN32
#include "native_texture.hpp"
#include "native_surfaces.hpp"
#endif
#include <limits>
namespace srhd_awa::matrixgame::unmanaged_texture {
using namespace platform3g;
#ifdef _WIN32
namespace {
IDirect3DDevice9& Device() {
    auto* provider=dynamic_cast<D3D9Device*>(&ReadyDevice());
    if(!provider)throw Unsupported("unmanaged texture operation requires actual D3D9 provider");
    const auto handles=provider->ReadLegacyHandles();
    if(!handles.device)throw Unsupported("unmanaged texture SDK identity unavailable");
    return *reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
}
#endif
DeviceResult Describe(IDirect3DTexture9* texture,std::uint32_t level,ResourceDesc* output) {
    if(!texture || !output)throw std::invalid_argument("null unmanaged texture description");
#ifdef _WIN32
    D3DSURFACE_DESC value{};const auto result=texture->GetLevelDesc(level,&value);
    if(SUCCEEDED(result))*output={value.Width,value.Height,static_cast<std::uint32_t>(value.Format),static_cast<std::uint32_t>(value.Pool),value.Usage};
    return result;
#else
    auto t=native_texture::Find(texture);auto& l=t->levels.at(level);
    *output={l.width,l.height,t->format,t->pool,t->usage};return 0;
#endif
}
DeviceResult Surface(IDirect3DTexture9* texture,std::uint32_t level,IDirect3DSurface9** output) {
    if(!texture || !output)throw std::invalid_argument("null texture surface query");
#ifdef _WIN32
    return texture->GetSurfaceLevel(level,output);
#else
    *output=nullptr;try{auto session=native_surfaces::Active();native_surfaces::Expose(session->Import(native_texture::Find(texture),level),output);return 0;}catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
#endif
}
DeviceResult Dirty(IDirect3DTexture9* texture,std::nullptr_t) {
    if(!texture)throw std::invalid_argument("null dirty texture");
#ifdef _WIN32
    return texture->AddDirtyRect(nullptr);
#else
    auto t=native_texture::Find(texture);if(t->usage&1u)throw Unsupported("render texture cannot use AddDirtyRect");t->dirty=true;return 0;
#endif
}
DeviceResult Update(IDirect3DTexture9* source,IDirect3DTexture9* destination) {
#ifdef _WIN32
    return Device().UpdateTexture(source,destination);
#else
    try{native_texture::Update(source,destination);return 0;}
    catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}
    catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
#endif
}
DeviceResult CopySurface(IDirect3DSurface9* destination,IDirect3DSurface9* source) {
#ifdef _WIN32
    return D3DXLoadSurfaceFromSurface(destination,nullptr,nullptr,source,nullptr,nullptr,D3DX_FILTER_NONE,0);
#else
    try{native_surfaces::Copy(source,destination);return 0;}catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
#endif
}
DeviceResult DecodeBitmap(const void* bytes,std::size_t size,std::uint32_t levels,IDirect3DTexture9** output) {
    if(!bytes || !size || size>std::numeric_limits<std::uint32_t>::max() || !output || *output)
        throw std::invalid_argument("invalid unmanaged bitmap input/output");
#ifdef _WIN32
    return D3DXCreateTextureFromFileInMemoryEx(&Device(),bytes,static_cast<UINT>(size),0,0,levels,0,
        D3DFMT_UNKNOWN,D3DPOOL_DEFAULT,D3DX_FILTER_TRIANGLE|D3DX_FILTER_DITHER,D3DX_FILTER_BOX,
        0,nullptr,nullptr,output);
#else
    (void)levels;(void)ReadyDevice();throw Unsupported("unmanaged bitmap SDK decode unavailable");
#endif
}
}

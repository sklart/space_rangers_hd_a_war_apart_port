// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#endif
#include "legacy_texture_backend.hpp"
#ifndef _WIN32
#include "native_texture.hpp"
#endif
#include "texture_asset.hpp"
#include <cstring>
#include <exception>
#include <limits>
namespace srhd_awa::matrixgame::legacy_texture {
namespace p=platform3g;
#ifdef _WIN32
namespace {
IDirect3DDevice9& Device() {
    const auto handles=p::ReadyDevice().ReadLegacyHandles();
    if(!handles.device)throw p::Unsupported("original texture requires a genuine SDK device");
    return *reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
IDirect3DTexture9& Texture(IDirect3DTexture9* texture) {
    if(!texture)throw std::invalid_argument("null original SDK texture");
    return *texture;
}
}
void Release(IDirect3DTexture9* texture)noexcept {if(texture)texture->Release();}
void Preload(IDirect3DTexture9* texture){Texture(texture).PreLoad();}
std::uint32_t SetPriority(IDirect3DTexture9* texture,std::uint32_t priority){return Texture(texture).SetPriority(priority);}
std::uint32_t LevelCount(IDirect3DTexture9* texture){return Texture(texture).GetLevelCount();}
p::DeviceResult Describe(IDirect3DTexture9* texture,std::uint32_t level,LevelDesc* out) {
    D3DSURFACE_DESC desc{};const auto result=Texture(texture).GetLevelDesc(level,&desc);
    if(SUCCEEDED(result))*out={desc.Width,desc.Height,static_cast<std::uint32_t>(desc.Format)};
    return static_cast<p::DeviceResult>(result);
}
p::DeviceResult Lock(IDirect3DTexture9* texture,std::uint32_t level,LockedRect* out,std::nullptr_t,std::uint32_t flags) {
    D3DLOCKED_RECT rect{};const auto result=Texture(texture).LockRect(level,&rect,nullptr,flags);
    if(SUCCEEDED(result))*out={rect.Pitch,rect.pBits};
    return static_cast<p::DeviceResult>(result);
}
p::DeviceResult LockRegion(IDirect3DTexture9* texture,std::uint32_t level,LockedRect* out,const TextureRect& region,std::uint32_t flags) {
    const RECT requested{region.left,region.top,region.right,region.bottom};
    D3DLOCKED_RECT rect{};const auto result=Texture(texture).LockRect(level,&rect,&requested,flags);
    if(SUCCEEDED(result))*out={rect.Pitch,rect.pBits};
    return static_cast<p::DeviceResult>(result);
}
p::DeviceResult Unlock(IDirect3DTexture9* texture,std::uint32_t level){return static_cast<p::DeviceResult>(Texture(texture).UnlockRect(level));}
p::DeviceResult Create(std::uint32_t width,std::uint32_t height,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t pool,IDirect3DTexture9** out,std::uintptr_t shared) {
    if(shared)throw p::Unsupported("shared original texture ownership unsupported");
    return static_cast<p::DeviceResult>(Device().CreateTexture(width,height,levels,usage,static_cast<D3DFORMAT>(format),static_cast<D3DPOOL>(pool),out,nullptr));
}
p::DeviceResult Decode(const void* bytes,std::size_t size,std::uint32_t levels,std::uint32_t pool,IDirect3DTexture9** out) {
    (void)Device(); // No decode/GPU success before actual SDK negotiation.
    auto asset=TextureAsset::Decode({static_cast<const std::byte*>(bytes),size});
    asset.PrepareMips(levels==1?TextureMipPolicy::OneLevel:TextureMipPolicy::CompleteBox);
    D3DFORMAT format=D3DFMT_A8R8G8B8;
    if(asset.format==TextureFormat::BC1)format=D3DFMT_DXT1;
    else if(asset.format==TextureFormat::BC2)format=D3DFMT_DXT3;
    else if(asset.format==TextureFormat::BC3)format=D3DFMT_DXT5;
    IDirect3DTexture9* texture=nullptr;
    auto result=Create(asset.levels.front().width,asset.levels.front().height,static_cast<std::uint32_t>(asset.levels.size()),0,format,pool,&texture,0);
    if(result!=p::DeviceSuccess)return result;
    struct Owner {IDirect3DTexture9* texture;~Owner(){Release(texture);}} owner{texture};
    for(std::uint32_t index=0;index<asset.levels.size();++index) {
        const auto& source=asset.levels[index];LockedRect locked{};
        result=Lock(texture,index,&locked,nullptr,0);if(result!=p::DeviceSuccess)return result;
        const auto rows=asset.format==TextureFormat::RGBA8?source.height:(source.height+3)/4;
        if(locked.Pitch<0||std::uint32_t(locked.Pitch)<source.pitch) {
            Unlock(texture,index);throw std::runtime_error("SDK texture pitch shorter than decoded row");
        }
        for(std::uint32_t row=0;row<rows;++row) {
            const auto* input=source.bytes.data()+std::size_t(row)*source.pitch;
            auto* output=static_cast<std::byte*>(locked.pBits)+std::size_t(row)*locked.Pitch;
            if(asset.format==TextureFormat::RGBA8) {
                for(std::uint32_t pixel=0;pixel<source.width;++pixel) {
                    output[pixel*4]=input[pixel*4+2];output[pixel*4+1]=input[pixel*4+1];
                    output[pixel*4+2]=input[pixel*4];output[pixel*4+3]=input[pixel*4+3];
                }
            } else std::memcpy(output,input,source.pitch);
        }
        result=Unlock(texture,index);if(result!=p::DeviceSuccess)return result;
    }
    *out=texture;owner.texture=nullptr;return p::DeviceSuccess;
}
p::DeviceResult DecodeFormatted(const void* bytes,std::size_t size,std::uint32_t levels,std::uint32_t format,
                                std::uint32_t pool,IDirect3DTexture9** out) {
    if(!out || *out || !bytes || !size || size>std::numeric_limits<UINT>::max())
        throw std::invalid_argument("invalid formatted texture input/output");
    IDirect3DTexture9* candidate=nullptr;
    // Exact original formatted bitmap loader request, including both filters.
    const auto result=D3DXCreateTextureFromFileInMemoryEx(&Device(),bytes,static_cast<UINT>(size),
        0,0,levels,0,static_cast<D3DFORMAT>(format),static_cast<D3DPOOL>(pool),
        D3DX_FILTER_NONE,D3DX_FILTER_BOX,0,nullptr,nullptr,&candidate);
    if(result!=D3D_OK){Release(candidate);return static_cast<p::DeviceResult>(result);}
    if(!candidate)throw std::runtime_error("formatted texture loader returned no texture");
    *out=candidate;return static_cast<p::DeviceResult>(result);
}
#else
namespace {
template<class F>p::DeviceResult AllocateCall(F&& call){
    try{return call();}
    catch(const native_texture::ResidencyExhausted&){return native_texture::OutOfVideoMemory;}
    catch(const std::bad_alloc&){return native_texture::OutOfMemory;}
}
}
void Release(IDirect3DTexture9* t)noexcept {native_texture::Release(t);}
void Preload(IDirect3DTexture9* t){native_texture::Preload(t);}
std::uint32_t SetPriority(IDirect3DTexture9* t,std::uint32_t v){return native_texture::Priority(t,v);}
std::uint32_t LevelCount(IDirect3DTexture9* t){return native_texture::Levels(t);}
p::DeviceResult Describe(IDirect3DTexture9* t,std::uint32_t l,LevelDesc* out){return native_texture::Describe(t,l,out);}
p::DeviceResult Lock(IDirect3DTexture9* t,std::uint32_t l,LockedRect* out,std::nullptr_t,std::uint32_t f){return native_texture::Lock(t,l,out,nullptr,f);}
p::DeviceResult LockRegion(IDirect3DTexture9* t,std::uint32_t l,LockedRect* out,const TextureRect& r,std::uint32_t f){return native_texture::Lock(t,l,out,&r,f);}
p::DeviceResult Unlock(IDirect3DTexture9* t,std::uint32_t l){return native_texture::Unlock(t,l);}
p::DeviceResult Create(std::uint32_t w,std::uint32_t h,std::uint32_t l,std::uint32_t u,std::uint32_t f,std::uint32_t pool,IDirect3DTexture9** out,std::uintptr_t shared){if(shared)throw p::Unsupported("shared native texture unsupported");return AllocateCall([&]{return native_texture::Create(w,h,l,u,f,pool,out);});}
p::DeviceResult Decode(const void* b,std::size_t n,std::uint32_t l,std::uint32_t pool,IDirect3DTexture9** out){return AllocateCall([&]{return native_texture::Decode(b,n,l,0,pool,out);});}
p::DeviceResult DecodeFormatted(const void* b,std::size_t n,std::uint32_t l,std::uint32_t f,std::uint32_t pool,IDirect3DTexture9** out){return AllocateCall([&]{return native_texture::Decode(b,n,l,f,pool,out);});}
#endif
}

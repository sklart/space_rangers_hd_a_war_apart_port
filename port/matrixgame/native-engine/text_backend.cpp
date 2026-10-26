// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include <d3dx9tex.h>
#include "Resource.h"
#include "platform_win32.hpp"
#endif
#include "text_backend.hpp"
#include "offscreen_backend.hpp"
#include <iostream>
namespace srhd_awa::matrixgame::text_backend {
namespace p=platform3g;
void LogError(std::string_view message){std::clog<<"MatrixGame Text: "<<message<<'\n';}
#ifdef _WIN32
namespace {HANDLE registered_font{};}
IDirect3DDevice9* DeviceIdentity() {
    auto* provider=dynamic_cast<p::D3D9Device*>(&p::ReadyDevice());
    if(!provider)throw p::Unsupported("text requires actual D3D9 provider");
    const auto handles=provider->ReadLegacyHandles();
    if(!handles.device)throw p::Unsupported("text SDK device unavailable");
    return reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
void PrepareRangersFont() {
    if(registered_font)return;
    const auto module=GetModuleHandleW(nullptr);
    if(!module){LogError("Failed to get module handle");return;}
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(IDF_RANGERS_FONT),MAKEINTRESOURCEW(8));
    if(!resource){LogError("Failed to find font resource");return;}
    const auto loaded=LoadResource(module,resource);
    if(!loaded){LogError("Failed to load font resource");return;}
    const auto locked=LockResource(loaded);
    if(!locked){LogError("Failed to lock font resource");return;}
    const auto bytes=SizeofResource(module,resource);
    if(!bytes){LogError("Failed to get font resource size");return;}
    DWORD count=0;registered_font=AddFontMemResourceEx(locked,bytes,nullptr,&count);
    if(!registered_font)LogError("Failed to add font");
}
void ReleaseRangersFont()noexcept {if(registered_font){RemoveFontMemResourceEx(registered_font);registered_font=nullptr;}}
p::DeviceResult CreateTextFont(IDirect3DDevice9* device,std::wstring_view name,std::size_t size,ID3DXFont** out) {
    if(!device)throw std::invalid_argument("null text SDK device");
    return D3DXCreateFontW(device,static_cast<INT>(size),0,FW_NORMAL,1,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,name.data(),out);
}
p::DeviceResult DescribeFont(ID3DXFont* font,FontDesc* out) {
    if(!font || !out)throw std::invalid_argument("null text font descriptor");
    D3DXFONT_DESCW value{};const auto result=font->GetDescW(&value);
    if(SUCCEEDED(result))out->Height=value.Height;return result;
}
bool MeasureSpace(ID3DXFont* font,const wchar_t* text,int count,Size* out) {
    if(!font || !out)throw std::invalid_argument("null text space measurement");
    SIZE value{};const auto result=GetTextExtentPoint32W(font->GetDC(),text,count,&value);
    out->cx=value.cx;out->cy=value.cy;return result!=FALSE;
}
p::DeviceResult GetSurface(IDirect3DTexture9* texture,std::uint32_t level,IDirect3DSurface9** out) {
    if(!texture)throw std::invalid_argument("null text texture");return texture->GetSurfaceLevel(level,out);
}
p::DeviceResult CreateRenderSurface(std::uint32_t width,std::uint32_t height,ID3DXRenderToSurface** out) {
    return D3DXCreateRenderToSurface(DeviceIdentity(),width,height,D3DFMT_A8R8G8B8,FALSE,D3DFMT_UNKNOWN,out);
}
p::DeviceResult BeginScene(ID3DXRenderToSurface* renderer,IDirect3DSurface9* surface) {
    if(!renderer)throw std::invalid_argument("null text render surface");return renderer->BeginScene(surface,nullptr);
}
p::DeviceResult EndScene(ID3DXRenderToSurface* renderer,std::uint32_t flags) {
    if(!renderer)throw std::invalid_argument("null text render surface");return renderer->EndScene(flags);
}
void Release(ID3DXRenderToSurface* renderer)noexcept {if(renderer)renderer->Release();}
#endif
#ifdef _WIN32
int DrawFontText(ID3DXFont* font,const wchar_t* text,int count,Rect* bounds,std::uint32_t flags,std::uint32_t color) {
    return debug_font::DrawTextWide(font,text,count,bounds,flags,color);
}
void Release(ID3DXFont* font)noexcept {debug_font::Release(font);}
#endif
void Release(IDirect3DTexture9* texture)noexcept {legacy_texture::Release(texture);}
#ifdef _WIN32
void Release(IDirect3DSurface9* surface)noexcept {offscreen::Release(surface);}
#endif
p::DeviceResult CreateTexture(std::uint32_t width,std::uint32_t height,IDirect3DTexture9** out){return legacy_texture::Create(width,height,0,0,21,1,out,0);}
p::DeviceResult Lock(IDirect3DTexture9* texture,std::uint32_t level,legacy_texture::LockedRect* out){return legacy_texture::Lock(texture,level,out,nullptr,0);}
p::DeviceResult Unlock(IDirect3DTexture9* texture,std::uint32_t level){return legacy_texture::Unlock(texture,level);}
#ifdef _WIN32
p::DeviceResult Clear(){return offscreen::Clear(0,nullptr,1,0,0,0);}
#endif
}

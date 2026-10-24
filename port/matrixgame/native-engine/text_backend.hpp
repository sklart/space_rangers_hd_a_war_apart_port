// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "debug_font_backend.hpp"
#include "legacy_texture_backend.hpp"
struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct ID3DXRenderToSurface;
namespace srhd_awa::matrixgame::text_backend {
using Rect=debug_font::TextRect;
struct FontDesc {std::int32_t Height;};
struct Size {std::int32_t cx{},cy{};};
IDirect3DDevice9* DeviceIdentity();
void LogError(std::string_view);
void PrepareRangersFont();
void ReleaseRangersFont()noexcept;
platform3g::DeviceResult CreateTextFont(IDirect3DDevice9*,std::wstring_view,std::size_t,ID3DXFont**);
platform3g::DeviceResult DescribeFont(ID3DXFont*,FontDesc*);
bool MeasureSpace(ID3DXFont*,const wchar_t*,int,Size*);
int DrawFontText(ID3DXFont*,const wchar_t*,int,Rect*,std::uint32_t,std::uint32_t);
void Release(ID3DXFont*)noexcept;
void Release(IDirect3DTexture9*)noexcept;
void Release(IDirect3DSurface9*)noexcept;
void Release(ID3DXRenderToSurface*)noexcept;
platform3g::DeviceResult CreateTexture(std::uint32_t,std::uint32_t,IDirect3DTexture9**);
platform3g::DeviceResult GetSurface(IDirect3DTexture9*,std::uint32_t,IDirect3DSurface9**);
platform3g::DeviceResult Lock(IDirect3DTexture9*,std::uint32_t,legacy_texture::LockedRect*);
platform3g::DeviceResult Unlock(IDirect3DTexture9*,std::uint32_t);
platform3g::DeviceResult CreateRenderSurface(std::uint32_t,std::uint32_t,ID3DXRenderToSurface**);
platform3g::DeviceResult BeginScene(ID3DXRenderToSurface*,IDirect3DSurface9*);
platform3g::DeviceResult EndScene(ID3DXRenderToSurface*,std::uint32_t);
platform3g::DeviceResult Clear();
}

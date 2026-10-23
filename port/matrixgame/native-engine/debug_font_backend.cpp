// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include "platform_win32.hpp"
#endif
#include "debug_font_backend.hpp"
namespace srhd_awa::matrixgame::debug_font {
namespace p=platform3g;
#ifdef _WIN32
namespace {
IDirect3DDevice9& ActualDevice(p::Device& source) {
    auto* provider=dynamic_cast<p::D3D9Device*>(&source);
    if(!provider)throw p::Unsupported("debug font requires the actual D3D9 provider");
    const auto handles=provider->ReadLegacyHandles();
    if(!handles.device)throw p::Unsupported("debug font SDK device unavailable");
    return *reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
ID3DXFont& Font(ID3DXFont* font) {
    if(!font)throw std::invalid_argument("null debug font");return *font;
}
static_assert((DT_NOCLIP|DT_LEFT|DT_VCENTER|DT_SINGLELINE)==0x124);
}
p::DeviceResult CreateFor(p::Device& device,ID3DXFont** out) {
    if(!out || *out)throw std::invalid_argument("debug font output must be empty");
    // Exact original CMatrixDebugInfo constructor request.
    const auto result=D3DXCreateFontW(&ActualDevice(device),25,0,FW_NORMAL,1,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH,L"MS Sans Serif",out);
    if(result==D3D_OK && !*out)throw std::runtime_error("D3DXCreateFontW returned no font");
    if(result!=D3D_OK && *out){(*out)->Release();*out=nullptr;}
    return static_cast<p::DeviceResult>(result);
}
void Release(ID3DXFont* font)noexcept {if(font)font->Release();}
int DrawText(ID3DXFont* font,const wchar_t* text,int count,TextRect& bounds,std::uint32_t flags,std::uint32_t color) {
    RECT rect{bounds.left,bounds.top,bounds.right,bounds.bottom};
    const int result=Font(font).DrawTextW(nullptr,text,count,&rect,flags,color);
    bounds={rect.left,rect.top,rect.right,rect.bottom};return result;
}
p::DeviceResult Lost(ID3DXFont* font){return static_cast<p::DeviceResult>(Font(font).OnLostDevice());}
p::DeviceResult Reset(ID3DXFont* font){return static_cast<p::DeviceResult>(Font(font).OnResetDevice());}
p::DeviceResult DrawPrimitiveUP(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride) {
    return static_cast<p::DeviceResult>(ActualDevice(p::ReadyDevice()).DrawPrimitiveUP(
        static_cast<D3DPRIMITIVETYPE>(type),count,data,stride));
}
#else
p::DeviceResult CreateFor(p::Device&,ID3DXFont**){throw p::Unsupported("debug font SDK creation unavailable on this backend");}
void Release(ID3DXFont* font)noexcept {if(font)std::terminate();}
int DrawText(ID3DXFont*,const wchar_t*,int,TextRect&,std::uint32_t,std::uint32_t){throw p::Unsupported("debug font draw backend unavailable");}
p::DeviceResult Lost(ID3DXFont*){throw p::Unsupported("debug font lost-device backend unavailable");}
p::DeviceResult Reset(ID3DXFont*){throw p::Unsupported("debug font reset backend unavailable");}
p::DeviceResult DrawPrimitiveUP(std::uint32_t,std::uint32_t,const void*,std::uint32_t){throw p::Unsupported("debug font immediate draw backend unavailable");}
#endif
p::DeviceResult Create(ID3DXFont** out) {
    try{return CreateFor(p::ReadyDevice(),out);}
    catch(const p::Unsupported& error){throw p::Unsupported(std::string("original debug font Create: ")+error.what());}
}
}

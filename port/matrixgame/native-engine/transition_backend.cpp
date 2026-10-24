// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#endif
#include "transition_backend.hpp"
namespace srhd_awa::matrixgame::transition {
namespace p=platform3g;
p::WindowRect ClientRect(){return p::ReadyWindow().ReadClientRect();}
#ifdef _WIN32
CaptureDC::CaptureDC(){
    window_=reinterpret_cast<std::uintptr_t>(p::ReadyWindow().Identity());
    dc_=reinterpret_cast<std::uintptr_t>(::GetDC(reinterpret_cast<HWND>(window_)));
    if(!dc_)throw std::runtime_error("transition GetDC failed");
}
void CaptureDC::Release()noexcept{
    if(dc_){::ReleaseDC(reinterpret_cast<HWND>(window_),reinterpret_cast<HDC>(dc_));dc_=0;}
}
void Bitmap::Clear()noexcept{
    if(dc_){::DeleteDC(reinterpret_cast<HDC>(dc_));dc_=0;}
    if(bitmap_){::DeleteObject(reinterpret_cast<HBITMAP>(bitmap_));bitmap_=0;}
}
void Bitmap::Save(bool save16as32){
    if(!dc_ || !bitmap_)return;
    struct {struct {BITMAPV4HEADER bmiHeader;} bmi;std::uint32_t pal[256];} b{};
    b.bmi.bmiHeader.bV4Size=sizeof(BITMAPINFOHEADER);
    const auto dc=reinterpret_cast<HDC>(dc_);const auto bitmap=reinterpret_cast<HBITMAP>(bitmap_);
    if(::GetDIBits(dc,bitmap,0,0,nullptr,reinterpret_cast<BITMAPINFO*>(&b.bmi),DIB_RGB_COLORS)==0)return;
    if(b.bmi.bmiHeader.bV4BitCount!=16 && b.bmi.bmiHeader.bV4BitCount!=24 && b.bmi.bmiHeader.bV4BitCount!=32)return;
    if(save16as32 && b.bmi.bmiHeader.bV4BitCount==16)b.bmi.bmiHeader.bV4BitCount=32;
    if(b.bmi.bmiHeader.bV4BitCount==16)Create16(b.bmi.bmiHeader.bV4Width,b.bmi.bmiHeader.bV4Height);
    else if(b.bmi.bmiHeader.bV4BitCount==24)CreateRGB(b.bmi.bmiHeader.bV4Width,b.bmi.bmiHeader.bV4Height);
    else CreateRGBA(b.bmi.bmiHeader.bV4Width,b.bmi.bmiHeader.bV4Height);
    Fill(Base::CPoint(0,0),Size(),0);
    if(::GetDIBits(dc,bitmap,0,b.bmi.bmiHeader.bV4Height,Data(),reinterpret_cast<BITMAPINFO*>(&b.bmi),DIB_RGB_COLORS)==0)return;
    FlipY();
    if(b.bmi.bmiHeader.bV4BitCount==24 || b.bmi.bmiHeader.bV4BitCount==32)SwapByte(Base::CPoint(0,0),Size(),0,2);
}
std::uintptr_t CompatibleBitmap(std::uintptr_t dc,int w,int h){return reinterpret_cast<std::uintptr_t>(::CreateCompatibleBitmap(reinterpret_cast<HDC>(dc),w,h));}
std::uintptr_t CompatibleDC(std::uintptr_t dc){return reinterpret_cast<std::uintptr_t>(::CreateCompatibleDC(reinterpret_cast<HDC>(dc)));}
std::uintptr_t SelectBitmap(std::uintptr_t dc,std::uintptr_t bitmap){return reinterpret_cast<std::uintptr_t>(::SelectObject(reinterpret_cast<HDC>(dc),reinterpret_cast<HBITMAP>(bitmap)));}
bool CopyPixels(std::uintptr_t d,int x,int y,int w,int h,std::uintptr_t s,int sx,int sy,std::uint32_t op){return ::BitBlt(reinterpret_cast<HDC>(d),x,y,w,h,reinterpret_cast<HDC>(s),sx,sy,op)!=0;}
p::DeviceResult Present(){
    auto* provider=dynamic_cast<p::D3D9Device*>(&p::ReadyDevice());
    if(!provider)throw p::Unsupported("transition requires actual D3D9 provider");
    return reinterpret_cast<IDirect3DDevice9*>(provider->ReadLegacyHandles().device)->Present(nullptr,nullptr,nullptr,nullptr);
}
#else
CaptureDC::CaptureDC(){(void)p::ReadyWindow();throw p::Unsupported("transition GDI screen capture unavailable");}
void CaptureDC::Release()noexcept{if(dc_)std::terminate();}
void Bitmap::Clear()noexcept{if(dc_ || bitmap_)std::terminate();}
void Bitmap::Save(bool){throw p::Unsupported("transition GDI bitmap readback unavailable");}
std::uintptr_t CompatibleBitmap(std::uintptr_t,int,int){throw p::Unsupported("transition compatible bitmap unavailable");}
std::uintptr_t CompatibleDC(std::uintptr_t){throw p::Unsupported("transition compatible DC unavailable");}
std::uintptr_t SelectBitmap(std::uintptr_t,std::uintptr_t){throw p::Unsupported("transition bitmap selection unavailable");}
bool CopyPixels(std::uintptr_t,int,int,int,int,std::uintptr_t,int,int,std::uint32_t){throw p::Unsupported("transition screen BitBlt unavailable");}
p::DeviceResult Present(){(void)p::ReadyDevice();throw p::Unsupported("transition presentation unavailable");}
#endif
}

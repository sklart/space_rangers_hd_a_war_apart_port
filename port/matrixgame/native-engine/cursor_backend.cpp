// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#endif
#include "cursor_backend.hpp"
#include <cstring>
namespace srhd_awa::matrixgame::cursor {
namespace p=platform3g;
#ifdef _WIN32
void BitmapFrame::Clear()noexcept {
    if(dc_){::DeleteDC(reinterpret_cast<HDC>(dc_));dc_=0;}
    if(bitmap_){::DeleteObject(reinterpret_cast<HBITMAP>(bitmap_));bitmap_=0;}
}
void BitmapFrame::Init() {
    Clear();
    if(Format()!=1)return; // Original CBitmap.cpp: BMF_FLAT=1, private to that TU.
    BITMAPV4HEADER bmi{};
    bmi.bV4Size=sizeof(bmi);bmi.bV4Width=SizeX();bmi.bV4Height=SizeY();
    bmi.bV4Planes=1;bmi.bV4BitCount=std::uint16_t(BitPP());
    if(BytePP()>=3){bmi.bV4V4Compression=BI_RGB;bmi.bV4SizeImage=0;}
    else {
        bmi.bV4V4Compression=BI_BITFIELDS;bmi.bV4RedMask=0x0000f800;
        bmi.bV4GreenMask=0x000007e0;bmi.bV4BlueMask=0x0000001f;
        bmi.bV4AlphaMask=0;bmi.bV4SizeImage=SizeX()*SizeY()*BytePP();
    }
    const auto screen=::GetDC(nullptr);
    struct ScreenOwner{HDC value;~ScreenOwner(){::ReleaseDC(nullptr,value);}} screen_owner{screen};
    void* bits{};
    bitmap_=reinterpret_cast<std::uintptr_t>(::CreateDIBSection(screen,reinterpret_cast<BITMAPINFO*>(&bmi),DIB_RGB_COLORS,&bits,nullptr,0));
    if(!bitmap_)throw std::runtime_error("cursor CreateDIBSection failed");
    const bool swap=(BytePP()==3 || BytePP()==4) && ColorMask(0)==0x0ff && ColorMask(1)==0x0ff00 && ColorMask(2)==0x0ff0000;
    if(swap)SwapByte(Base::CPoint(0,0),Size(),0,2);
    const auto line=std::uint32_t(SizeX()*BytePP());const auto stride=(line+3)&~3u;
    auto* destination=static_cast<std::uint8_t*>(bits)+stride*std::uint32_t(SizeY()-1);
    auto* source=Data();
    for(int y=0;y<SizeY();++y){std::memcpy(destination,source,SizeX()*BytePP());source+=line;destination-=stride;}
    if(swap)SwapByte(Base::CPoint(0,0),Size(),0,2);
    dc_=reinterpret_cast<std::uintptr_t>(::CreateCompatibleDC(screen));
    if(!dc_)throw std::runtime_error("cursor CreateCompatibleDC failed");
    if(!::SelectObject(reinterpret_cast<HDC>(dc_),reinterpret_cast<HBITMAP>(bitmap_)))throw std::runtime_error("cursor SelectObject failed");
}
p::IconHandle CreateIcon(const IconInfo& value) {
    ICONINFO info{};info.fIcon=value.fIcon;info.xHotspot=value.xHotspot;info.yHotspot=value.yHotspot;
    info.hbmMask=reinterpret_cast<HBITMAP>(value.hbmMask);info.hbmColor=reinterpret_cast<HBITMAP>(value.hbmColor);
    return {reinterpret_cast<std::uintptr_t>(::CreateIconIndirect(&info))};
}
int ShowSystemCursor(bool show){(void)p::ReadyWindow();return ::ShowCursor(show);}
bool ShowDeviceCursor(bool show) {
    const auto handles=p::ReadyDevice().ReadLegacyHandles();
    if(!handles.device)throw p::Unsupported("cursor requires genuine SDK device");
    return reinterpret_cast<IDirect3DDevice9*>(handles.device)->ShowCursor(show)!=FALSE;
}
#else
void BitmapFrame::Clear()noexcept {if(bitmap_ || dc_)std::terminate();}
void BitmapFrame::Init(){throw p::Unsupported("cursor GDI bitmap unavailable");}
p::IconHandle CreateIcon(const IconInfo&){throw p::Unsupported("cursor system icon creation unavailable");}
int ShowSystemCursor(bool){throw p::Unsupported("cursor system display counter unavailable");}
bool ShowDeviceCursor(bool){(void)p::ReadyDevice();throw p::Unsupported("cursor SDK visibility unavailable");}
#endif
}

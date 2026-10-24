// Independent SDK observation of icons and the actual HAL target.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_3g.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
void VerifyCursorIcon(std::uintptr_t value,unsigned frame) {
    ICONINFO info{};assert(GetIconInfo(reinterpret_cast<HICON>(value),&info));
    struct Owner{ICONINFO& v;~Owner(){DeleteObject(v.hbmMask);DeleteObject(v.hbmColor);}} owner{info};
    assert(!info.fIcon && info.xHotspot==1 && info.yHotspot==1);
    BITMAP shape{};assert(GetObjectW(info.hbmColor,sizeof(shape),&shape));assert(shape.bmWidth==4 && shape.bmHeight==4);
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=4;bitmap.bmiHeader.biHeight=-4;bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    std::uint32_t pixels[16]{};const auto dc=GetDC(nullptr);assert(GetDIBits(dc,info.hbmColor,0,4,pixels,&bitmap,DIB_RGB_COLORS)==4);ReleaseDC(nullptr,dc);
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)assert((pixels[y*4+x]&0xffffffu)==(((0x40+0x30*frame)<<16)|((0x20+0x10*y)<<8)|(0x10+8*x)));
}
std::uint64_t CursorGuiCounts(){return (std::uint64_t(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS))<<32)|GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);}
void WarmCursorGdi() {
    const auto before=CursorGuiCounts();
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=4;bitmap.bmiHeader.biHeight=-4;bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    const auto dc=GetDC(nullptr);void* bits{};
    const auto image=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&bits,nullptr,0);assert(image && bits);
    const auto memory=CreateCompatibleDC(dc);assert(memory);assert(SelectObject(memory,image));
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)static_cast<std::uint32_t*>(bits)[y*4+x]=0xff400000u|((0x20+0x10*y)<<8)|(0x10+8*x);
    ICONINFO info{};info.xHotspot=1;info.yHotspot=1;info.hbmMask=image;info.hbmColor=image;
    const auto icon=CreateIconIndirect(&info);assert(icon);VerifyCursorIcon(reinterpret_cast<std::uintptr_t>(icon),0);
    assert(DestroyIcon(icon));assert(DeleteDC(memory));assert(DeleteObject(image));assert(ReleaseDC(nullptr,dc));
    std::printf("independent SDK GDI warmup before=%llu after=%llu\n",static_cast<unsigned long long>(before),static_cast<unsigned long long>(CursorGuiCounts()));
}
int ReadCursorDisplayCount(){ShowCursor(TRUE);return ShowCursor(FALSE);}
void RestoreCursorDisplayCount(int expected){auto count=ReadCursorDisplayCount();while(count<expected)count=ShowCursor(TRUE);while(count>expected)count=ShowCursor(FALSE);}
void VerifyCursorPixels() {
    namespace p=srhd_awa::matrixgame::platform3g;
    auto* device=reinterpret_cast<IDirect3DDevice9*>(p::ReadyDevice().ReadLegacyHandles().device);assert(device);
    IDirect3DSurface9 *target{},*copy{};assert(SUCCEEDED(device->GetRenderTarget(0,&target)));
    struct Owner{IDirect3DSurface9*& a;IDirect3DSurface9*& b;~Owner(){if(a)a->Release();if(b)b->Release();}} owner{target,copy};
    D3DSURFACE_DESC desc{};assert(SUCCEEDED(target->GetDesc(&desc)));
    assert(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr)));
    assert(SUCCEEDED(device->GetRenderTargetData(target,copy)));
    D3DLOCKED_RECT locked{};assert(SUCCEEDED(copy->LockRect(&locked,nullptr,D3DLOCK_READONLY)));
    unsigned colored=0;
    for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x)if((reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(locked.pBits)+y*locked.Pitch)[x]&0xffffffu)!=0)++colored;
    assert(SUCCEEDED(copy->UnlockRect()));assert(colored==16);
}

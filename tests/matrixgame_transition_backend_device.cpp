// Actual Windows GDI capture/readback; no simulated bitmap or device.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "transition_backend.hpp"
#include <cassert>
#include <cstdio>
namespace t=srhd_awa::matrixgame::transition;
int VerifyOriginalTransitionCapture(){
    const auto rect=t::ClientRect();assert(rect.right>rect.left && rect.bottom>rect.top);
    for(int entry=0;entry<3;++entry){
        t::CaptureDC screen;
        t::Bitmap source,destination;
        for(auto* bitmap:{&source,&destination}){
            bitmap->SetHandle(t::CompatibleBitmap(screen,16,16));assert(bitmap->GetHandle());
            bitmap->SetDC(t::CompatibleDC(screen));assert(bitmap->GetDC());
            assert(t::SelectBitmap(bitmap->GetDC(),bitmap->GetHandle()));
        }
        const auto brush=::CreateSolidBrush(RGB(0x12,0x34,0x56));assert(brush);
        RECT area{0,0,16,16};assert(::FillRect(reinterpret_cast<HDC>(source.GetDC()),&area,brush));
        assert(::DeleteObject(brush));
        const auto top=::CreateSolidBrush(RGB(0x9a,0xbc,0xde));assert(top);
        RECT top_area{0,0,16,8};assert(::FillRect(reinterpret_cast<HDC>(source.GetDC()),&top_area,top));
        assert(::DeleteObject(top));
        assert(t::CopyPixels(destination.GetDC(),0,0,16,16,source.GetDC(),0,0,SRCCOPY));
        screen.Release();screen.Release();
        destination.Save(true);
        assert(destination.SizeX()==16 && destination.SizeY()==16);
        assert(destination.BytePP()==3 || destination.BytePP()==4);
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){
            const auto* pixel=destination.Data()+y*destination.Pitch()+x*destination.BytePP();
            assert(pixel[0]==(y<8?0x9a:0x12) && pixel[1]==(y<8?0xbc:0x34) && pixel[2]==(y<8?0xde:0x56));
        }
        destination.Clear();destination.Clear();
    }
    std::puts("actual transition GDI bitmap capture/readback RGB bytes and cleanup/reentry PASS; full CTransition BuildTexture NOT EXECUTED");
    return 0;
}

// Actual GDI/system/device operations; full MatrixCursor runtime is a separate gate.
#include "cursor_backend.hpp"
#include <cassert>
#include <cstdio>
void VerifyCursorIcon(std::uintptr_t,unsigned);
std::uint64_t CursorGuiCounts();
int ReadCursorDisplayCount();
void RestoreCursorDisplayCount(int);
void WarmCursorGdi();
void VerifyCursorBackendDevice() {
    namespace c=srhd_awa::matrixgame::cursor;
    namespace p=srhd_awa::matrixgame::platform3g;
    auto& window=p::ReadyWindow();const auto old=window.ReadCursor();
    const auto display=ReadCursorDisplayCount();
    struct Owner{p::Window& window;p::CursorHandle old;int count;~Owner(){window.ExchangeCursor(old);RestoreCursorDisplayCount(count);}} owner{window,old,display};
    assert(c::ShowSystemCursor(false)==display-1);assert(c::ShowSystemCursor(true)==display);
    (void)c::ShowDeviceCursor(false);(void)c::ShowDeviceCursor(true);
    WarmCursorGdi();const auto gui=CursorGuiCounts();
    for(unsigned cycle=0;cycle<2;++cycle)for(unsigned frame=0;frame<3;++frame) {
        c::BitmapFrame bitmap;bitmap.CreateRGBA(4,4);
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
            auto* pixel=bitmap.Data()+y*bitmap.Pitch()+x*4;
            pixel[0]=std::uint8_t(0x40+0x30*frame);pixel[1]=std::uint8_t(0x20+0x10*y);pixel[2]=std::uint8_t(0x10+8*x);pixel[3]=255;
        }
        const auto first=bitmap.Data()[0];bitmap.Init();assert(bitmap.Data()[0]==first && bitmap.GetHandle());
        const c::IconInfo info{false,1,1,bitmap.GetHandle(),bitmap.GetHandle()};
        const auto icon=c::CreateIcon(info);assert(icon.value);
        window.ExchangeCursor({icon.value});VerifyCursorIcon(window.ReadCursor().value,frame);
        window.ExchangeCursor(old);window.DestroyCursorIcon(icon);
        bitmap.Clear();bitmap.Clear();assert(!bitmap.GetHandle());
        const auto counts=CursorGuiCounts();
        std::printf("cursor GDI probe cycle=%u frame=%u before=%llu after=%llu restored=%d\n",cycle,frame,
                    static_cast<unsigned long long>(gui),static_cast<unsigned long long>(counts),window.ReadCursor()==old);
    }
    assert(CursorGuiCounts()==gui && window.ReadCursor()==old);
    std::puts("actual cursor backend: original WinBitmap Init/Clear algorithm, GDI RGB rows/hotspot, six owned icons, system counter/device visibility and handle cleanup PASS; full MatrixCursor NOT EXECUTED");
}

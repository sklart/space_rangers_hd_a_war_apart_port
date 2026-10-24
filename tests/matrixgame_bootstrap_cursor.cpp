// Actual original cursor under the borrowed Windows HAL/window harness.
#include "Texture.hpp"
#include "MatrixCursor.hpp"
#include "MatrixConfig.hpp"
#include "MatrixGame.h"
#include "MatrixInstantDraw.hpp"
#include "offscreen_backend.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
void VerifyCursorIcon(std::uintptr_t,unsigned);
void VerifyCursorPixels();
std::uint64_t CursorGuiCounts();
int ReadCursorDisplayCount();
void RestoreCursorDisplayCount(int);
void WarmCursorGdi();
void VerifyOriginalCursorDevice(const wchar_t* path) {
    namespace p=srhd_awa::matrixgame::platform3g;
    namespace b=srhd_awa::matrixgame::buffers;
    namespace os=srhd_awa::matrixgame::offscreen;
    assert(!g_MatrixHeap && !g_Cache && !g_CacheHeap && !g_Config.m_Cursors);
    Base::CHeap heap;g_MatrixHeap=&heap;
    struct HeapOwner{~HeapOwner(){g_MatrixHeap=nullptr;}} heap_owner;
    CacheInit();struct CacheOwner{~CacheOwner(){CacheDeinit();}} cache_owner;
    const auto display_count=ReadCursorDisplayCount();
    struct DisplayOwner{int value;~DisplayOwner(){RestoreCursorDisplayCount(value);}} display_owner{display_count};
    CBitmap atlas;atlas.CreateRGBA(12,4);
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<12;++x) {
        auto* pixel=atlas.Data()+y*atlas.Pitch()+x*4;
        pixel[0]=std::uint8_t(0x40+0x30*(x/4));pixel[1]=std::uint8_t(0x20+0x10*y);pixel[2]=std::uint8_t(0x10+8*(x%4));pixel[3]=255;
    }
    assert(atlas.SaveInPNG(path));
    g_Config.m_Cursors=static_cast<SStringPair*>(HAlloc(2*sizeof(SStringPair),g_CacheHeap));g_Config.m_CursorsCnt=0;
    struct ConfigOwner{~ConfigOwner(){g_Config.Clear();}} config_owner;
    for(unsigned i=0;i<2;++i){new(g_Config.m_Cursors+i)SStringPair{};++g_Config.m_CursorsCnt;}
    g_Config.m_Cursors[0].key=L"cursor-test-system";g_Config.m_Cursors[0].val=std::wstring(path)+L"?1,1,3,4,10";
    g_Config.m_Cursors[1].key=L"cursor-test-software";g_Config.m_Cursors[1].val=std::wstring(path)+L"?1,1,-3,4,10";
    auto& window=p::ReadyWindow();const auto original=window.ReadCursor();
    WarmCursorGdi(); // Independent SDK cold initialization precedes handle baseline.
    b::Context staging;b::Scope binding(staging);
    CInstDraw::StaticInit();struct BatchOwner{~BatchOwner(){CInstDraw::ClearAll();}} batch_owner;
    for(int cycle=0;cycle<2;++cycle) {
        g_Config.m_SoftwareCursor=false;
        const auto gui=CursorGuiCounts();
        CMatrixCursor cursor;assert(window.ReadCursor().value==0);
        cursor.Select(L"cursor-test-system");cursor.SetVisible(true);
        VerifyCursorIcon(window.ReadCursor().value,0);
        cursor.Takt(1);VerifyCursorIcon(window.ReadCursor().value,1);
        cursor.Takt(10);VerifyCursorIcon(window.ReadCursor().value,2);
        cursor.Takt(10);VerifyCursorIcon(window.ReadCursor().value,0);
        cursor.SetVisible(false);cursor.Draw();cursor.Clear();cursor.Clear();
        assert(window.ReadCursor()==original && CursorGuiCounts()==gui);
        g_Config.m_SoftwareCursor=true;cursor.Select(L"cursor-test-software");cursor.SetPos(8,8);cursor.SetVisible(true);cursor.Takt(10);
        p::CheckDeviceResult(os::Clear(0,nullptr,1,0xff000000u,1,0),"cursor background clear");
        p::CheckDeviceResult(os::BeginScene(),"cursor scene begin");cursor.Draw();p::CheckDeviceResult(os::EndScene(),"cursor scene end");
        VerifyCursorPixels();cursor.SetVisible(false);cursor.Clear();cursor.Clear();assert(window.ReadCursor()==original);
        CInstDraw::ClearAll();CInstDraw::StaticInit();
    }
    std::puts("original cursor system Select/SetVisible/Takt/Clear, GDI icon pixels/hotspot/counts, software Select/Takt/Draw with real HAL pixels and reentry PASS");
}

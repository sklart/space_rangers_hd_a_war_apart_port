// SDK-free caller: catch Windows macro-dependent ABI names across TUs.
#include "debug_font_backend.hpp"
#include "MatrixDebugInfo.hpp"
#include "renderer_ops.hpp"
#include <cassert>
#include <cstdio>
void VerifyOriginalDebugFontOwner() {
    for(int cycle=0;cycle<2;++cycle) {
        CMatrixDebugInfo debug;
        debug.T(L"bootstrap",L"actual font owner",20,0);
        debug.Takt(1);
        debug.OnLostDevice();debug.OnResetDevice();
        auto& device=srhd_awa::matrixgame::platform3g::ReadyDevice();
        std::uint32_t before[6]{};
        for(unsigned key=0;key<6;++key)
            before[key]=device.ReadTextureStageState(0,static_cast<srhd_awa::matrixgame::platform3g::TextureStageRegister>(key+1));
        auto state=device.CaptureState();
        SetColorOpSelect(0,3u);SetAlphaOpSelect(0,3u);
        const std::uint32_t expected[6]{2,3,3,2,3,3};
        for(unsigned key=0;key<6;++key)
            assert(device.ReadTextureStageState(0,static_cast<srhd_awa::matrixgame::platform3g::TextureStageRegister>(key+1))==expected[key]);
        assert(state->Restore()==srhd_awa::matrixgame::platform3g::DeviceSuccess);state.reset();
        for(unsigned key=0;key<6;++key)
            assert(device.ReadTextureStageState(0,static_cast<srhd_awa::matrixgame::platform3g::TextureStageRegister>(key+1))==before[key]);
    }
    std::puts("original CMatrixDebugInfo ctor/T/Takt/lost/reset/dtor and shared ordinary color/alpha select ops EXECUTED on negotiated real device, two owners; Draw and physical GPU pixels NOT TESTED");
}
void VerifyPortableDebugFontText(ID3DXFont* font) {
    namespace df=srhd_awa::matrixgame::debug_font;
    df::TextRect bounds{};
    // DT_CALCRECT | DT_SINGLELINE, checked against the real SDK by the caller.
    const auto height=df::DrawTextWide(font,L"MatrixGame",-1,bounds,0x420u,0xffffffffu);
    assert(height>0 && bounds.right>bounds.left && bounds.bottom>bounds.top);
}

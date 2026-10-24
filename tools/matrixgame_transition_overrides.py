"""Complete original transition algorithm with real SDK capture boundaries."""
import hashlib,re
from matrixgame_render_pipeline_overrides import adapt

def prepare_transition(output):
    text=(output/'MatrixGame/src/MatrixTransition.cpp').read_text(encoding='utf-8')
    text=adapt(text)
    replacements={
        'HDC hdc = GetDC(g_Wnd);':'transition_native::CaptureDC hdc;',
        'RECT r;\n    GetClientRect(g_Wnd, &r);':'const auto r=transition_native::ClientRect();',
        'WinBitmap bm;':'transition_native::Bitmap bm;',
        'CreateCompatibleBitmap(':'transition_native::CompatibleBitmap(',
        'CreateCompatibleDC(':'transition_native::CompatibleDC(',
        'SelectObject(':'transition_native::SelectBitmap(',
        'BitBlt(':'transition_native::CopyPixels(',
        'ReleaseDC(g_Wnd, hdc);':'hdc.Release();',
        'SRCCOPY':'0x00cc0020u',
        'D3DFMT_DXT1':'0x31545844u',
        'g_D3DD->BeginScene()':'transition_offscreen::BeginScene()',
        'g_D3DD->EndScene()':'transition_offscreen::EndScene()',
        'g_D3DD->Present(NULL, NULL, NULL, NULL)':'transition_native::Present()',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DZB_FALSE':'0u',
        'D3DTEXF_LINEAR':'2u','D3DTEXF_NONE':'0u',
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter',
        'D3DSAMP_MIPFILTER':'srhd_awa::matrixgame::SamplerRegister::MipFilter',
    }
    for old,new in replacements.items():
        if old not in text:raise ValueError('Original transition boundary changed: '+old)
        text=text.replace(old,new)
    prelude='''#include "Texture.hpp"
#include "transition_backend.hpp"
#include "offscreen_backend.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace transition_native=srhd_awa::matrixgame::transition;
namespace transition_offscreen=srhd_awa::matrixgame::offscreen;
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call),#call)
#endif
'''
    name='MatrixGame/src/OriginalTransition.cpp';p=output/name
    p.write_bytes((prelude+text).encode());return {name:hashlib.sha256(p.read_bytes()).hexdigest()}

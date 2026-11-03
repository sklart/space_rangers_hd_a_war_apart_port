"""Complete original transition algorithm with real SDK capture boundaries."""
import hashlib,re
from matrixgame_render_pipeline_overrides import adapt

def prepare_transition(output):
    text=(output/'MatrixGame/src/MatrixTransition.cpp').read_text(encoding='utf-8')
    text=adapt(text)
    replacements={
        'HDC hdc = GetDC(g_Wnd);':'',
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
    begin='    bm.SetHandle(transition_native::CompatibleBitmap(hdc, r.right, r.bottom));'
    end='    bm.Save(true);'
    if text.count(begin)!=1 or text.count(end)!=1:raise ValueError('Original transition capture block changed')
    first=text.index(begin);last=text.index(end,first)+len(end)
    text=text[:first]+'    transition_native::CaptureLoadingImage(bm, m_ScreenX, m_ScreenY);'+text[last:]
    old='''    CBlockPar *tra = g_MatrixData->BlockGetNE(L"Transitions");
    if (tra == NULL)
        return;
    tra = tra->BlockGetNE(utils::format(L"%d.%d", m_ScreenX, m_ScreenY).c_str());

    if (tra == NULL)
        return;'''
    new='''    diagnostic::Operation geometry_trace("B.transition.BuildTexture");
    CBlockPar *transitions = g_MatrixData->BlockGetNE(L"Transitions");
    if (!transitions) throw rp3g::Unsupported("missing authored transition data");
    const auto geometry=transition_native::ResolveGeometry(m_ScreenX,m_ScreenY,
        [&](const std::wstring& name){return transitions->BlockGetNE(name);});
    CBlockPar *tra=geometry.block;
    diagnostic::Record("metadata","B.transition.geometry",
        geometry.width==m_ScreenX && geometry.height==m_ScreenY?"exact authored layout":"scaled authored layout");
    diagnostic::Metric("transition.source_width",geometry.width);
    diagnostic::Metric("transition.source_height",geometry.height);
    diagnostic::Metric("transition.target_width",m_ScreenX);
    diagnostic::Metric("transition.target_height",m_ScreenY);
    diagnostic::Metric("transition.geometry_count",tra->ParCount());'''
    if text.count(old)!=1:raise ValueError('Original transition layout lookup changed')
    text=text.replace(old,new)
    # Scale authored coordinates BEFORE deriving UVs for the native screenshot.
    # Scale velocity too: target resize must not change animation duration.
    for axis,field in ((0,'x'),(1,'y')):
        pattern=r'(?m)^(\s*m_Geom\[i\]\.verts\[[0-3]\]\.p\.'+field+r' = [^\r\n;]+);$'
        text,count=re.subn(pattern,r'\1 * geometry.'+field+'_scale;',text)
        if count!=4:raise ValueError('Original transition coordinates changed')
    marker='        m_Geom[i].dir *= (float)tra->ParGetName(i).GetStrPar(1, L",").GetDouble();'
    if text.count(marker)!=1:raise ValueError('Original transition velocity changed')
    text=text.replace(marker,marker+'\n        m_Geom[i].dir.x *= geometry.x_scale;\n        m_Geom[i].dir.y *= geometry.y_scale;')
    # Capture can fail before m_Tex is published. Always release partial geometry
    # during the existing Clear/Deinit path, including that failure case.
    old='''        HFree(m_Geom, g_MatrixHeap);
        m_Geom = NULL;
        m_GeomCnt = 0;
    }
}'''
    new='''    }
    if(m_Geom) HFree(m_Geom, g_MatrixHeap);
    m_Geom = NULL;
    m_GeomCnt = 0;
}'''
    if text.count(old)!=1:raise ValueError('Original transition cleanup changed')
    text=text.replace(old,new)
    prelude='''#include "Texture.hpp"
#include "transition_backend.hpp"
#include "transition_geometry.hpp"
#include "native_diagnostic_journal.hpp"
#include "offscreen_backend.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace transition_native=srhd_awa::matrixgame::transition;
namespace transition_offscreen=srhd_awa::matrixgame::offscreen;
namespace diagnostic=srhd_awa::matrixgame::diagnostic;
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call),#call)
#endif
'''
    name='MatrixGame/src/OriginalTransition.cpp';p=output/name
    p.write_bytes((prelude+text).encode());return {name:hashlib.sha256(p.read_bytes()).hexdigest()}

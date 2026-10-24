"""Full original debug-info algorithm; exact transition cleanup for map unwind."""
import hashlib
from matrixgame_vector_object_overrides import function


def prepare_debug_font(output):
    changes={}
    def write(name,text):
        p=output/name;p.write_bytes(text.encode());changes[name]=hashlib.sha256(p.read_bytes()).hexdigest()
    name='MatrixGame/src/MatrixDebugInfo.cpp';text=(output/name).read_text(encoding='utf-8')
    sdk='#include <d3dx9core.h>'
    if text.count(sdk)!=1:raise ValueError('Original debug font SDK include changed')
    text=text.replace(sdk,'#include "debug_font_backend.hpp"\nnamespace df=srhd_awa::matrixgame::debug_font;')
    original='''D3DXCreateFontW(g_D3DD, 25, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  PROOF_QUALITY, DEFAULT_PITCH, L"MS Sans Serif", &m_Font)'''
    if text.count(original)!=1:raise ValueError('Original debug font request changed')
    text=text.replace(original,'df::Create(&m_Font)').replace('D3D_OK !=','0 !=')
    text=text.replace('extern IDirect3DDevice9* g_D3DD;','')
    for old,new,count in [('m_Font->Release()','df::Release(m_Font)',1),
        ('m_Font->OnLostDevice()','df::Lost(m_Font)',1),('m_Font->OnResetDevice()','df::Reset(m_Font)',1),
        ('m_Font->DrawTextW(NULL,','df::DrawTextWide(m_Font,',2),('(LPRECT)&r','&r',2)]:
        if text.count(old)!=count:raise ValueError('Original font boundary changed: '+old)
        text=text.replace(old,new)
    flags='DT_NOCLIP | DT_LEFT | DT_VCENTER | DT_SINGLELINE'
    flags2='DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP'
    if text.count(flags)!=1 or text.count(flags2)!=1:raise ValueError('Original text flags changed')
    text=text.replace(flags,'0x124u').replace(flags2,'0x124u')
    text=text.replace('g_D3DD->SetRenderState(D3DRS_TEXTUREFACTOR,','srhd_awa::matrixgame::platform3g::ReadyDevice().SetRenderState(static_cast<srhd_awa::matrixgame::RasterRegister>(60),')
    text=text.replace('g_D3DD->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)','srhd_awa::matrixgame::platform3g::ReadyDevice().SetRenderState(srhd_awa::matrixgame::RasterRegister::BlendEnable, 1u)')
    text=text.replace('g_D3DD->SetFVF(','srhd_awa::matrixgame::platform3g::ReadyDevice().SetFixedVertexLayout(')
    text=text.replace('g_D3DD->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,','df::DrawPrimitiveUP(5u,')
    text=text.replace('D3DFVF_XYZRHW','srhd_awa::matrixgame::vertex_layout::TransformedPosition')
    text=text.replace('D3DTA_TFACTOR','3u')
    # Windows FLOAT is exactly float; retain the original arithmetic/casts.
    if text.count('static_cast<FLOAT>')!=2:raise ValueError('Original debug geometry casts changed')
    text=text.replace('static_cast<FLOAT>','static_cast<float>')
    write(name,text)
    # Retain the historical exact cleanup slice for source audits. The native
    # target now selects OriginalTransition.cpp, prepared separately from the
    # complete original capture/render TU; do not link both Clear definitions.
    original=(output/'MatrixGame/src/MatrixTransition.cpp').read_text(encoding='utf-8')
    clear=function(original,'void CTransition::Clear(void)')
    write('MatrixGame/src/MatrixTransitionLifecycle.cpp',
        '#include "3g.hpp"\n#include "Texture.hpp"\n#include "MatrixMap.hpp"\n'+clear+'\n')
    return changes

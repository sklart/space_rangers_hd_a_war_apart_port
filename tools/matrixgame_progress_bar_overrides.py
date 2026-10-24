"""Compile original progress bars with the genuine typed texture/device owners."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_progress_bar(output):
    name='MatrixGame/src/MatrixProgressBar.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    old='(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, TEXTURE_PATH_PB)'
    if text.count(old)!=1:raise ValueError('Original progress texture factory changed')
    text=text.replace(old,'pb_texture::GetManaged(TEXTURE_PATH_PB)')
    text=adapt(text)
    values={'D3DRS_ALPHAREF':'static_cast<srhd_awa::matrixgame::RasterRegister>(24)',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DZB_FALSE':'0u','D3DTEXF_POINT':'1u','D3DTEXF_NONE':'0u',
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter',
        'D3DSAMP_MIPFILTER':'srhd_awa::matrixgame::SamplerRegister::MipFilter'}
    for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    marker='CMatrixProgressBar::CMatrixProgressBar(void)'
    if text.count(marker)!=1:raise ValueError('Original progress constructor changed')
    text=text.replace(marker,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''+marker,1)
    prelude='''#include "MatrixGame.h"
#include "Texture.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace pb_texture=srhd_awa::matrixgame::legacy_texture;
'''
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}

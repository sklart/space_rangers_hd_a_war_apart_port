"""Compile the complete original interface element through existing device adapters."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_interface_element(output):
    name='MatrixGame/src/Interface/CIFaceElement.cpp';path=output/name
    text=adapt(path.read_text(encoding='utf-8'))
    clear='::srhd_awa::matrixgame::ZeroTrivialBytes(m_Actions, sizeof(m_Actions));'
    if text.count(clear)!=1:raise ValueError('Original interface action initialization changed')
    text=text.replace(clear,'for(auto& action:m_Actions) action.m_function = nullptr;')
    values={
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter',
        'D3DSAMP_MIPFILTER':'srhd_awa::matrixgame::SamplerRegister::MipFilter',
        'D3DTEXF_POINT':'1u','D3DTEXF_NONE':'0u',
        'D3DRS_ALPHAREF':'static_cast<srhd_awa::matrixgame::RasterRegister>(24)',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DRS_MULTISAMPLEANTIALIAS':'static_cast<srhd_awa::matrixgame::RasterRegister>(161)',
        'D3DZB_FALSE':'0u','D3DZB_TRUE':'1u',
    }
    for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    marker='CIFaceElement::CIFaceElement(void)'
    if text.count(marker)!=1:raise ValueError('Original interface element owner changed')
    prelude='''#include "Texture.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
'''
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}

"""Connect real effect/map buffer declarations to the existing staging owner."""
import re
TYPES={
    'D3D_VB':'::srhd_awa::matrixgame::buffers::VertexBuffer*',
    'D3D_IB':'::srhd_awa::matrixgame::buffers::IndexBuffer*',
    'D3DVIEWPORT9':'::srhd_awa::matrixgame::Viewport',
    'LPDIRECT3DTEXTURE9':'IDirect3DTexture9*',
    'D3DRECT':'::srhd_awa::matrixgame::RectI',
    'ZeroMemory':'::srhd_awa::matrixgame::ZeroTrivialBytes',
}
CALLS={
    'CREATE_VB':'NATIVE_BUF_CREATE_VB','CREATE_VB_DYNAMIC':'NATIVE_BUF_CREATE_DYNAMIC_VB','CREATE_VBM':'NATIVE_BUF_CREATE_MANAGED_VB',
    'CREATE_IB16':'NATIVE_BUF_CREATE_IB16','CREATE_IBM16':'NATIVE_BUF_CREATE_MANAGED_IB','CREATE_IBD16':'NATIVE_BUF_CREATE_DYNAMIC_IB','CREATE_IB32':'NATIVE_BUF_CREATE_IB32',
    'IS_VB':'NATIVE_BUF_IS','IS_IB':'NATIVE_BUF_IS','DESTROY_VB':'NATIVE_BUF_DESTROY','DESTROY_IB':'NATIVE_BUF_DESTROY',
    'GET_VB':'NATIVE_BUF_GET','GET_IB':'NATIVE_BUF_GET','LOCK_VB':'NATIVE_BUF_LOCK','LOCK_IB':'NATIVE_BUF_LOCK',
    'LOCKP_VB':'NATIVE_BUF_LOCK_PART','LOCKP_IB':'NATIVE_BUF_LOCK_PART','UNLOCK_VB':'NATIVE_BUF_UNLOCK','UNLOCK_IB':'NATIVE_BUF_UNLOCK',
    'LOCKD_IB':'NATIVE_BUF_LOCK_DISCARD','LOCK_VB_DYNAMIC':'NATIVE_BUF_LOCK_DISCARD','LOCKPD_IB':'NATIVE_BUF_LOCK_PART_DISCARD',
    'LOCKP_VB_DYNAMIC':'NATIVE_BUF_LOCK_PART_DISCARD','LOCKP_VB_NO':'NATIVE_BUF_LOCK_PART_NOOVERWRITE',
}
LAYOUTS={
    'D3DFVF_XYZ':'Position','D3DFVF_XYZRHW':'TransformedPosition','D3DFVF_XYZB1':'PositionBlend1',
    'D3DFVF_NORMAL':'Normal','D3DFVF_DIFFUSE':'Diffuse','D3DFVF_SPECULAR':'Specular',
    'D3DFVF_TEX1':'Texture1','D3DFVF_TEX2':'Texture2','D3DFVF_TEX3':'Texture3','D3DFVF_TEXCOORDSIZE3':'TexcoordSize3',
}

def patch_resource_header(name,data):
    # Leave the original SDK/debug implementation in the source view. Native
    # clients import the existing bridge instead; no counterfeit SDK header.
    if name=='MatrixLib/3G/D3DControl.hpp':return data
    text=data.decode();before=text
    if name=='MatrixLib/3G/CBillboard.hpp':
        text=text.replace('IDirect3DVertexBuffer9* m_VB','::srhd_awa::matrixgame::buffers::VertexBuffer* m_VB')
        text=text.replace('IDirect3DIndexBuffer9* m_IB','::srhd_awa::matrixgame::buffers::IndexBuffer* m_IB')
    text=text.replace('#include "D3DControl.hpp"','#include "big_buffer_bridge.hpp"')
    mapping={**TYPES,**CALLS,**{k:'::srhd_awa::matrixgame::vertex_layout::'+v for k,v in LAYOUTS.items()}}
    for old,new in mapping.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    if text!=before:text='#include "big_buffer_bridge.hpp"\n#include "math_types.hpp"\n#include "memory_ops.hpp"\n'+text
    if '__stdcall' in text:
        text=re.sub(r'\b__stdcall\b','MATRIXGAME_NATIVE_CALLBACK',text)
        text='#include "engine_callconv.hpp"\n'+text
    return text.encode()

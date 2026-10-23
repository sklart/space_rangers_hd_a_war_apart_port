"""Compile the complete original renderer with explicit typed device operations.

Keep all water/terrain callbacks and their branch/pass selection. SDK enum
values below are serialized D3D9 identifiers, not a fabricated device API.
"""
import hashlib
import re

VALUES = {
    'D3DBLEND_DESTALPHA':7,'D3DBLEND_INVDESTALPHA':8,'D3DBLEND_INVSRCALPHA':6,
    'D3DBLEND_ONE':2,'D3DBLEND_SRCALPHA':5,'D3DBLEND_SRCCOLOR':3,'D3DBLEND_ZERO':1,
    'D3DTADDRESS_CLAMP':3,'D3DTADDRESS_WRAP':1,
    'D3DTA_CURRENT':1,'D3DTA_DIFFUSE':0,'D3DTA_TEMP':5,'D3DTA_TEXTURE':2,'D3DTA_TFACTOR':3,
    'D3DTOP_ADD':7,'D3DTOP_BLENDCURRENTALPHA':16,'D3DTOP_BLENDFACTORALPHA':14,
    'D3DTOP_BLENDTEXTUREALPHA':13,'D3DTOP_MODULATE':4,'D3DTOP_MODULATE2X':5,
    'D3DTOP_SELECTARG1':2,'D3DTOP_DISABLE':1,
    'D3DTSS_TCI_CAMERASPACENORMAL':0x10000,'D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR':0x30000,
    'D3DTSS_TCI_PASSTHRU':0,'D3DTTFF_COUNT2':2,'D3DTTFF_DISABLE':0,
}
RENDER={'ALPHABLENDENABLE':27,'ALPHATESTENABLE':15,'AMBIENT':139,'COLORWRITEENABLE':168,
        'DESTBLEND':20,'LIGHTING':137,'SRCBLEND':19,'TEXTUREFACTOR':60,'ZWRITEENABLE':14}
STAGES={'COLOROP':1,'COLORARG1':2,'COLORARG2':3,'ALPHAOP':4,'ALPHAARG1':5,'ALPHAARG2':6,
        'TEXCOORDINDEX':11,'RESULTARG':28,'TEXTURETRANSFORMFLAGS':24}
OPS=('SetColorOp','SetAlphaOp','SetColorOpAnyOrder','SetAlphaOpAnyOrder',
     'SetColorOpSelect','SetAlphaOpSelect','SetColorOpDisable','SetAlphaOpDisable')

def adapt(text):
    mapping={**{k:str(v)+'u' for k,v in VALUES.items()},
             **{'D3DRS_'+k:'static_cast<srhd_awa::matrixgame::RasterRegister>('+str(v)+')' for k,v in RENDER.items()},
             **{'D3DTSS_'+k:'static_cast<rp3g::TextureStageRegister>('+str(v)+')' for k,v in STAGES.items()},
             'D3DSAMP_ADDRESSU':'srhd_awa::matrixgame::SamplerRegister::AddressU',
             'D3DSAMP_ADDRESSV':'srhd_awa::matrixgame::SamplerRegister::AddressV',
             'TRUE':'true','FALSE':'false',
             'D3DDISPLAYMODE':'rp3g::DisplayMode','D3DFMT_X8R8G8B8':'rp3g::PixelFormat::Xrgb8',
             'D3DTEXTUREOP':'std::uint32_t','DWORD':'std::uint32_t'}
    for old,new in mapping.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    calls={'SetRenderState':'SetRenderState','SetSamplerState':'SetSamplerState',
           'SetTextureStageState':'SetTextureStageState','SetFVF':'SetFixedVertexLayout',
           'SetTexture':'BindBorrowedTexture','SetStreamSource':'BindStagingVertex','SetIndices':'BindStagingIndex'}
    for old,new in calls.items():text=text.replace('g_D3DD->'+old+'(', 'rp3g::ReadyDevice().'+new+'(')
    text=text.replace('g_D3DDCaps.MaxSimultaneousTextures','srhd_awa::matrixgame::render_pipeline::MaxSimultaneousTextures()')
    text=text.replace('g_D3DD->SetTransform(D3DTS_TEXTURE2,', 'srhd_awa::matrixgame::render_pipeline::TextureTransform(2,')
    text=text.replace('FAILED_DX(g_D3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm));',
                      'd3ddm = rp3g::ReadyDevice().ReadAdapterDisplayMode(0);')
    for name in OPS:text=re.sub(r'\b'+name+r'\b','original_renderer_ops::'+name,text)
    return text

def prepare_render_pipeline(output):
    changes={}
    def write(name,text):
        path=output/name;path.write_bytes(text.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    header='MatrixGame/src/MatrixRenderPipeline.hpp'
    h=(output/header).read_text(encoding='utf-8')
    h=h.replace('class CRenderPipeline : public CMain','class CRenderPipeline : public Base::CMain')
    write(header,h)
    # Ordinary color/alpha operations are the actual original inline algorithms.
    # Keep them local to this complete renderer TU; full 3g.cpp remains unported.
    original=(output/'MatrixLib/3G/3g.cpp').read_text(encoding='utf-8')
    start=original.index('#ifdef DO_SMART_COLOROPS')
    # Header preparation moved both original branches to the original CPP.
    ordinary=original.index('\n#else\nvoid SetColorOp(', start)
    end=original.index('\n#endif',ordinary)
    helpers=original[ordinary+len('\n#else\n'):end]
    helpers=adapt(helpers).replace('original_renderer_ops::','')
    prelude='''#ifdef DO_SMART_COLOROPS
#error Native renderer currently preserves the original ordinary color-op branch only.
#endif
#include "render_pipeline_backend.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace original_renderer_ops {
'''+helpers+'\n}\n'
    name='MatrixGame/src/MatrixRenderPipeline.cpp'
    text=(output/name).read_text(encoding='utf-8')
    # Checked HRESULT behavior stays !=0; no CExceptionD3D implementation needed.
    position=text.index('#define REND_TYPE0')
    text=text[:position]+'''#undef ASSERT_DX
#undef FAILED_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
'''+text[position:]
    text=adapt(text)
    # These legacy object callbacks have no header/export/external references;
    # all table assignments in the pinned constructor are commented out. Keep
    # their complete algorithms TU-local, so unused code/UBSan metadata is not
    # retained as a spurious dependency of the active water/terrain pipeline.
    # If a future in-TU caller is enabled, ordinary compilation emits the real
    # methods and their full sanitizer/type dependencies again.
    for callback in ('obj_ordinal_t4','obj_ordinal_gloss_t4'):
        signature='bool '+callback+'(uintptr_t user_param, int)'
        if text.count(signature)!=1:raise ValueError('Original object callback changed: '+callback)
        text=text.replace(signature,'static '+signature,1)
    write(name,prelude+text)
    return changes

"""Keep the entire original MatrixMap TU and route its real SDK boundaries."""
import hashlib
import re

RENDER = {
    'ZENABLE':7,'SHADEMODE':9,'ZWRITEENABLE':14,'ALPHATESTENABLE':15,
    'SRCBLEND':19,'DESTBLEND':20,'CULLMODE':22,'ALPHAREF':24,'ALPHAFUNC':25,
    'ALPHABLENDENABLE':27,'FOGENABLE':28,'FOGCOLOR':34,'FOGTABLEMODE':35,
    'FOGSTART':36,'FOGEND':37,'STENCILENABLE':52,'STENCILFAIL':53,
    'STENCILZFAIL':54,'STENCILPASS':55,'STENCILFUNC':56,'STENCILREF':57,
    'STENCILMASK':58,'STENCILWRITEMASK':59,'TEXTUREFACTOR':60,'LIGHTING':137,
    'AMBIENT':139,'FOGVERTEXMODE':140,'COLORWRITEENABLE':168,
    'TWOSIDEDSTENCILMODE':185,'CCW_STENCILFAIL':186,'CCW_STENCILZFAIL':187,
    'CCW_STENCILPASS':188,'CCW_STENCILFUNC':189,
}
VALUES = {
    'D3DTEXF_NONE':0,'D3DTEXF_POINT':1,'D3DTEXF_LINEAR':2,
    'D3DTADDRESS_WRAP':1,'D3DTADDRESS_CLAMP':3,
    'D3DBLEND_ZERO':1,'D3DBLEND_ONE':2,'D3DBLEND_SRCALPHA':5,'D3DBLEND_INVSRCALPHA':6,
    'D3DCMP_LESSEQUAL':4,'D3DCMP_GREATEREQUAL':7,'D3DCMP_ALWAYS':8,
    'D3DSTENCILOP_KEEP':1,'D3DSTENCILOP_REPLACE':3,'D3DSTENCILOP_INCR':7,'D3DSTENCILOP_DECR':8,
    'D3DCULL_NONE':1,'D3DCULL_CW':2,'D3DCULL_CCW':3,
    'D3DSHADE_FLAT':1,'D3DSHADE_GOURAUD':2,'D3DZB_FALSE':0,'D3DZB_TRUE':1,
    'D3DTA_DIFFUSE':0,'D3DTA_TEXTURE':2,'D3DTA_TFACTOR':3,'D3DTOP_MODULATE':4,
    'D3DPT_TRIANGLESTRIP':5,'D3DFOG_NONE':0,'D3DFOG_LINEAR':3,
    'D3DSTENCILCAPS_TWOSIDED':0x100,'D3DPRASTERCAPS_WFOG':0x100000,
}
SAMPLER = {'ADDRESSU':'AddressU','ADDRESSV':'AddressV','MAGFILTER':'MagFilter',
           'MINFILTER':'MinFilter','MIPFILTER':'MipFilter','MIPMAPLODBIAS':'LodBias'}


def prepare_map_device(output):
    name='MatrixGame/src/MatrixMap.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    # These are original float bit payloads, not numerical float->DWORD casts.
    for field,count in [('m_BiasTer',2),('m_BiasRobots',1),('m_BiasWater',1),('fBias',2)]:
        old='*((LPDWORD)(&'+('('+field+')' if field=='fBias' else field)+'))'
        if text.count(old)!=count:raise ValueError('Original LOD bit payload changed: '+field)
        text=text.replace(old,'std::bit_cast<std::uint32_t>('+field+')')
    for field in ('Start','End'):
        old='*(std::uint32_t *)(&'+field+')'
        if text.count(old)!=1:raise ValueError('Original fog bit payload changed')
        text=text.replace(old,'std::bit_cast<std::uint32_t>('+field+')')
    mapping={**{k:str(v)+'u' for k,v in VALUES.items()},
             **{'D3DRS_'+k:'static_cast<srhd_awa::matrixgame::RasterRegister>('+str(v)+')' for k,v in RENDER.items()},
             **{'D3DSAMP_'+k:'srhd_awa::matrixgame::SamplerRegister::'+v for k,v in SAMPLER.items()},
             **{'D3DTS_'+k:'map_device::Transform::'+k.title() for k in ('WORLD','VIEW','PROJECTION')},
             'TRUE':'true','FALSE':'false'}
    for old,new in mapping.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    for old,new in {'SetRenderState':'SetRenderState','SetSamplerState':'SetSamplerState',
                    'SetFVF':'SetFixedVertexLayout','SetTexture':'BindBorrowedTexture',
                    'SetStreamSource':'BindStagingVertex'}.items():
        text=text.replace('g_D3DD->'+old+'(','map_platform::ReadyDevice().'+new+'(')
    for method in ('SetTransform','DrawPrimitive'):
        text=text.replace('g_D3DD->'+method+'(','map_device::'+method+'(')
    for key in ('Stencil','Raster'):
        text=text.replace('g_D3DDCaps.'+key+'Caps','map_device::'+key+'Caps()')
    start=text.index('        PROCESS_INFORMATION pi0;')
    end=text.index('        CloseHandle(pi0.hThread);',start)+len('        CloseHandle(pi0.hThread);')
    text=text[:start]+'        map_device::OpenManual();'+text[end:]
    prelude='''#include "map_device_backend.hpp"
#include "ShadowProj.hpp"
#include <bit>
namespace map_platform=srhd_awa::matrixgame::platform3g;
namespace map_device=srhd_awa::matrixgame::map_device;
'''
    # Same original ASSERT_OFF branch and signed result check, without requiring
    # the otherwise unported CExceptionD3D implementation for retained methods.
    marker='#include "platform_3g.hpp"'
    if text.count(marker)!=1:raise ValueError('Map SDK include boundary changed')
    check='''
#undef ASSERT_DX
#undef FAILED_DX
#define FAILED_DX(call) map_platform::CheckDeviceResult((call), #call)
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''
    # Put the overrides after all includes, so 3g.hpp cannot redefine them.
    position=text.index('CMatrixMap::CMatrixMap')
    text=text[:position]+check+'\n'+text[position:]
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}

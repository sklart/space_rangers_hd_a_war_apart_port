"""Full original constructor and multi-selection with existing typed device operations."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_constructor(output):
    changes={}
    for name,marker in [('MatrixGame/src/Interface/CConstructor.cpp','CConstructor::CConstructor()'),
                        ('MatrixGame/src/MatrixMultiSelection.cpp','CMultiSelection::CMultiSelection(')]:
        path=output/name;text=adapt(path.read_text(encoding='utf-8'))
        values={
            'D3DVIEWPORT9':'rp3g::VideoViewport','D3DLIGHT9':'rp3g::VideoLight',
            'D3DLIGHT_DIRECTIONAL':'3u','D3DCLEAR_TARGET':'1u','D3DCLEAR_ZBUFFER':'2u','D3DCLEAR_STENCIL':'4u',
            'D3DCULL_NONE':'1u','D3DCULL_CCW':'3u','D3DFILL_SOLID':'3u','D3DFILL_WIREFRAME':'2u',
            'D3DCMP_ALWAYS':'8u','D3DCMP_EQUAL':'3u','D3DCMP_NOTEQUAL':'6u',
            'D3DSTENCILOP_KEEP':'1u','D3DSTENCILOP_ZERO':'2u','D3DSTENCILOP_REPLACE':'3u',
            'D3DFVF_XYZRHW':'4u','D3DPT_LINESTRIP':'3u','D3DX_PI':'DirectX::XM_PI',
        }
        for token,value in {'CULLMODE':22,'FILLMODE':8,'STENCILENABLE':52,'STENCILFAIL':53,'STENCILZFAIL':54,
                            'STENCILPASS':55,'STENCILFUNC':56,'STENCILREF':57,'STENCILMASK':58,'STENCILWRITEMASK':59}.items():
            values['D3DRS_'+token]=f'static_cast<srhd_awa::matrixgame::RasterRegister>({value})'
        for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
        text=text.replace('g_D3DD->SetViewport(','constructor_offscreen::SetViewport(')
        text=text.replace('g_D3DD->Clear(','constructor_offscreen::Clear(')
        text=text.replace('g_D3DD->DrawPrimitiveUP(','constructor_device::DrawPrimitiveUP(')
        text=text.replace('g_D3DD->SetLight(0, &light)','rp3g::ReadyDevice().SetVideoLight(0, light)')
        text=text.replace('D3DXMatrixLookAtLH(','srhd_awa::matrixgame::geometry::LookAtLH(')
        for kind in ['WORLD','VIEW','PROJECTION']:
            text=text.replace('g_D3DD->SetTransform(D3DTS_'+kind+',','constructor_device::SetTransform(constructor_device::Transform::'+kind.title()+',')
        text=text.replace('D3DCOLOR_XRGB(255, 0, 0)','0xffff0000u')
        text=text.replace('light.Direction = D3DXVECTOR3(-0.82242596f, 0.56887215f, 0);','light.Direction = {-0.82242596f, 0.56887215f, 0};')
        text=text.replace('light.Direction = g_MatrixMap->m_LightMain;','light.Direction = {g_MatrixMap->m_LightMain.x,g_MatrixMap->m_LightMain.y,g_MatrixMap->m_LightMain.z};')
        if text.count(marker)!=1:raise ValueError('Original constructor/selection owner changed: '+name)
        check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
        text=text.replace(marker,check+'\n'+marker,1)
        prelude='''#include "Texture.hpp"
#include "offscreen_backend.hpp"
#include "map_device_backend.hpp"
#include "renderer_ops.hpp"
#include "view_geometry.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace constructor_offscreen=srhd_awa::matrixgame::offscreen;
namespace constructor_device=srhd_awa::matrixgame::map_device;
'''
        path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes

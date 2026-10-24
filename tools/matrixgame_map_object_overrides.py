"""Compile the full original map-object TU through existing real adapters."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_map_object(output):
    name='MatrixGame/src/MatrixObject.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    lod='*((LPDWORD)(&m_TexBias))'
    if text.count(lod)!=1:raise ValueError('Original map object LOD bit payload changed')
    text=text.replace(lod,'std::bit_cast<std::uint32_t>(m_TexBias)')
    text=adapt(text)
    values={'D3DSAMP_MIPMAPLODBIAS':'srhd_awa::matrixgame::SamplerRegister::LodBias',
        'D3DRS_NORMALIZENORMALS':'static_cast<srhd_awa::matrixgame::RasterRegister>(143)'}
    for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    for kind in ['VIEW','WORLD','PROJECTION']:
        text=text.replace('g_D3DD->SetTransform(D3DTS_'+kind+',',
            'obj_device::SetTransform(obj_device::Transform::'+kind.title()+',')
    for old,new in {'D3DXMatrixRotationX':'obj_geometry::RotationX',
        'D3DXMatrixRotationY':'obj_geometry::RotationY',
        'D3DXMatrixScaling':'obj_geometry::Scaling',
        'D3DXVec3TransformNormal':'obj_geometry::TransformNormal',
        'D3DXMatrixOrthoLH':'srhd_awa::matrixgame::minimap_geometry::OrthoLH',
        'D3DXMatrixLookAtLH':'srhd_awa::matrixgame::geometry::LookAtLH'}.items():
        text=text.replace(old+'(',new+'(')
    # The existing GameStaticResources TU already owns the original statics.
    marker='void CMatrixMapObject::InitTextures(int n)'
    if text.count(marker)!=1:raise ValueError('Original map-object texture lifecycle changed')
    text=text.replace(marker,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''+marker,1)
    prelude='''#include "Texture.hpp"
#include "ShadowProj.hpp"
#include "renderer_ops.hpp"
#include "map_device_backend.hpp"
#include "object_geometry.hpp"
#include "view_geometry.hpp"
#include "minimap_geometry.hpp"
#include <bit>
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace obj_device=srhd_awa::matrixgame::map_device;
namespace obj_geometry=srhd_awa::matrixgame::object_geometry;
'''
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}

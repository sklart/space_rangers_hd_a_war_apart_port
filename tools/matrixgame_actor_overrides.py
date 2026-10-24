"""Full original Robot/Flyer owners; typed SDK operations, no fake RTTI."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_actors(output):
    changed={}
    for name,marker in [('MatrixGame/src/MatrixObjectRobot.cpp','CMatrixRobot::CMatrixRobot()'),
                        ('MatrixGame/src/MatrixFlyer.cpp','CMatrixFlyer::CMatrixFlyer(void)')]:
        path=output/name;text=path.read_text(encoding='utf-8')
        text=text.replace('(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged,','actor_texture::GetManaged(')
        text=re.sub(r'\*\(\(LPDWORD\)\(&([^\)]+)\)\)',r'std::bit_cast<std::uint32_t>(\1)',text)
        text=adapt(text)
        values={'D3DSAMP_MIPMAPLODBIAS':'srhd_awa::matrixgame::SamplerRegister::LodBias',
                'D3DRS_NORMALIZENORMALS':'static_cast<srhd_awa::matrixgame::RasterRegister>(143)',
                'D3DRS_CULLMODE':'static_cast<srhd_awa::matrixgame::RasterRegister>(22)',
                'D3DCULL_CW':'2u','D3DCULL_CCW':'3u','D3DPT_TRIANGLEFAN':'6u'}
        for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
        for old,new in {'D3DXMatrixRotationX':'actor_geometry::RotationX',
                        'D3DXMatrixRotationY':'actor_geometry::RotationY',
                        'D3DXMatrixScaling':'actor_geometry::Scaling',
                        'D3DXVec3TransformNormal':'actor_geometry::TransformNormal',
                        'D3DXVec2Dot':'actor_math::Dot2',
                        'D3DXVec2TransformCoord':'actor_math::TransformCoord2',
                        'D3DXPlaneTransform':'actor_math::TransformPlane'}.items():
            text=text.replace(old+'(',new+'(')
        text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,','actor_device::SetTransform(actor_device::Transform::World,')
        text=text.replace('g_D3DD->DrawPrimitive(','actor_device::DrawPrimitive(')
        text=text.replace('MoveMemory(','std::memmove(').replace('_alloca(','actor_alloca(')
        if text.count(marker)!=1:raise ValueError('Original actor owner changed: '+name)
        check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
        text=text.replace(marker,check+'\n'+marker,1)
        prelude='''#include "Texture.hpp"
#include "ShadowProj.hpp"
#include "renderer_ops.hpp"
#include "map_device_backend.hpp"
#include "actor_geometry.hpp"
#include <bit>
#include <cstring>
#ifdef _MSC_VER
#include <malloc.h>
#define actor_alloca _alloca
#else
#define actor_alloca __builtin_alloca
#endif
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace actor_device=srhd_awa::matrixgame::map_device;
namespace actor_geometry=srhd_awa::matrixgame::object_geometry;
namespace actor_math=srhd_awa::matrixgame::actor_geometry;
namespace actor_texture=srhd_awa::matrixgame::legacy_texture;
'''
        path.write_bytes((prelude+text).encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changed

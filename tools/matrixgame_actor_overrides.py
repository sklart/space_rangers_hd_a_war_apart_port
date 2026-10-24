"""Full original actor owners; typed SDK operations, no fake RTTI."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_actors(output):
    changed={}
    for name,marker in [('MatrixGame/src/MatrixObjectRobot.cpp','CMatrixRobot::CMatrixRobot()'),
                        ('MatrixGame/src/MatrixFlyer.cpp','CMatrixFlyer::CMatrixFlyer(void)'),
                        ('MatrixGame/src/MatrixObjectBuilding.cpp','CMatrixBuilding::CMatrixBuilding()'),
                        ('MatrixGame/src/MatrixRobot.cpp','CMatrixRobotAI::CMatrixRobotAI()'),
                        ('MatrixGame/src/MatrixObjectCannon.cpp','CMatrixCannon::CMatrixCannon(void)')]:
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
                        'D3DXMatrixRotationYawPitchRoll':'actor_geometry::RotationYawPitchRoll',
                        'D3DXMatrixTranslation':'actor_translation::Translation',
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
        if name.endswith('/MatrixObjectCannon.cpp'):
            multiply='D3DXMatrixMultiply(&m_Unit[i].m_Matrix, &mx, &m_Core->m_Matrix);'
            if text.count(multiply)!=1:raise ValueError('Original cannon matrix product changed')
            text=text.replace(multiply,'m_Unit[i].m_Matrix = mx * m_Core->m_Matrix;')
            clear='::srhd_awa::matrixgame::ZeroTrivialBytes(m_Unit, sizeof(m_Unit));'
            if text.count(clear)!=2:raise ValueError('Original cannon unit reset boundaries changed')
            text=text.replace(clear,'srhd_awa::matrixgame::cannon_units::Reset(m_Unit, MR_MAXCANNONUNIT);')
            reset='''void srhd_awa::matrixgame::cannon_units::Reset(SMatrixCannonUnit* units,std::size_t count) {
    for(std::size_t i=0;i<count;++i) {
        auto& unit=units[i];
        if(unit.m_Smoke.effect)unit.m_Smoke.Unconnect();
        unit.m_Type=CUT_EMPTY;
        unit.m_Graph=nullptr;
        unit.m_Matrix=D3DXMATRIX{};
        // The normal (non-DIP) branch is active after a reset. Its pointer
        // resources were released by UnitClear; never byte-clear the handler.
        new(&unit.u1.s1) decltype(unit.u1.s1){};
        unit.u1.s1.m_ShadowStencil=nullptr;
        unit.u1.s1.m_Angle=0;
        unit.u1.s1.m_LinkMatrix=0;
        unit.u1.s1.m_IMatrix=D3DXMATRIX{};
        unit.u1.s1.m_Invert=0;
    }
}

'''
            text=text.replace(marker,reset+marker,1)
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
#include "minimap_geometry.hpp"
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
namespace actor_translation=srhd_awa::matrixgame::minimap_geometry;
namespace actor_texture=srhd_awa::matrixgame::legacy_texture;
'''
        if name.endswith('/MatrixObjectCannon.cpp'):
            prelude+='#include "cannon_unit_lifecycle.hpp"\n#include <new>\n'
        path.write_bytes((prelude+text).encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changed

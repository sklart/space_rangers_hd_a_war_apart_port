"""Full original effects, point-light pool and billboard resource ownership."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_effects(output):
    changes={}
    units=[('MatrixGame/src/Effects/MatrixEffect.cpp','static const SBillboardID2Texname bb2tn[]'),
           ('MatrixGame/src/Effects/MatrixEffectPointLight.cpp','static bool UnloadDX('),
           ('MatrixLib/3G/CBillboard.cpp','void CBillboard::SortEndDraw('),
           ('MatrixGame/src/Effects/MatrixEffectSmokeAndFire.cpp','CMatrixEffectSmoke::CMatrixEffectSmoke('),
           ('MatrixGame/src/Effects/MatrixEffectExplosion.cpp','const SExplosionProperties ExplosionNormal ='),
           ('MatrixGame/src/Effects/MatrixEffectKonus.cpp','CMatrixEffectKonus::CMatrixEffectKonus('),
           ('MatrixGame/src/Effects/MatrixEffectLandscapeSpot.cpp','static SSpotProperties m_SpotProperties[SPOT_TYPES_CNT];'),
           ('MatrixGame/src/Effects/MatrixEffectWeapon.cpp','void CMatrixEffectWeapon::WeaponHit('),
           ('MatrixGame/src/Effects/MatrixEffectSelection.cpp','CMatrixEffectSelection::CMatrixEffectSelection(')]
    for name,marker in units:
        path=output/name;text=path.read_text(encoding='utf-8')
        text=text.replace('(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, ','effect_texture::GetManaged(')
        text=adapt(text)
        values={'D3DRS_ALPHAREF':'static_cast<srhd_awa::matrixgame::RasterRegister>(24)',
                'D3DRS_CULLMODE':'static_cast<srhd_awa::matrixgame::RasterRegister>(22)',
                'D3DCULL_NONE':'1u','D3DCULL_CCW':'3u','D3DPT_TRIANGLELIST':'4u',
                'D3DPT_TRIANGLESTRIP':'5u','D3DTA_COMPLEMENT':'0x10u','D3DTOP_SUBTRACT':'10u',
                'D3DSAMP_MIPMAPLODBIAS':'srhd_awa::matrixgame::SamplerRegister::LodBias'}
        for token,value in values.items():text=re.sub(r'\b'+token+r'\b',value,text)
        if name.endswith('/MatrixEffect.cpp'):
            bits='*((LPDWORD)(&(fBias)))'
            if text.count(bits)!=1:raise ValueError('Original effects LOD bias changed')
            text=text.replace(bits,'std::bit_cast<std::uint32_t>(fBias)')
        if name.endswith('/MatrixEffectPointLight.cpp'):
            if text.count('_alloca(')!=2:raise ValueError('Original point-light stack allocations changed')
            text=text.replace('_alloca(','effect_alloca(')
        if name.endswith('/MatrixEffectLandscapeSpot.cpp'):
            if text.count('_alloca(')!=1:raise ValueError('Original landscape stack allocation changed')
            text=text.replace('_alloca(','effect_alloca(')
            stores={
                '*(std::uint32_t *)idxs = *(idxs - 1) | (tv0->index << 16);':'effect_store_indices(idxs, *(idxs - 1) | (tv0->index << 16));',
                '*(std::uint32_t *)(idxs + 0) = tv0->index | (tv1->index << 16);':'effect_store_indices(idxs + 0, tv0->index | (tv1->index << 16));',
                '*(std::uint32_t *)(idxs + 2) = tv2->index | (tv3->index << 16);':'effect_store_indices(idxs + 2, tv2->index | (tv3->index << 16));'}
            for old,new in stores.items():
                if text.count(old)!=1:raise ValueError('Original landscape index packing changed: '+old)
                text=text.replace(old,new)
        text=text.replace('D3DXVec3TransformCoordArray(','srhd_awa::matrixgame::geometry::TransformCoordArray(')
        text=text.replace('D3DXMatrixRotationYawPitchRoll(','srhd_awa::matrixgame::object_geometry::RotationYawPitchRoll(')
        text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,','effect_device::SetTransform(effect_device::Transform::World,')
        text=text.replace('g_D3DD->DrawIndexedPrimitive(','effect_device::DrawIndexedPrimitive(')
        text=text.replace('g_D3DD->DrawPrimitive(','effect_device::DrawPrimitive(')
        text=text.replace('D3DPT_TRIANGLEFAN','6u')
        text=text.replace('g_D3DD->DrawPrimitiveUP(','srhd_awa::matrixgame::debug_font::DrawPrimitiveUP(')
        if text.count(marker)!=1:raise ValueError('Original effects entry changed: '+name)
        check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
        text=text.replace(marker,check+'\n'+marker)
        prelude='''#include "Texture.hpp"
#include "render_pipeline_backend.hpp"
#include "renderer_ops.hpp"
#include "map_device_backend.hpp"
#include "debug_font_backend.hpp"
#include "geometry_arrays.hpp"
#include <bit>
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace effect_texture=srhd_awa::matrixgame::legacy_texture;
namespace effect_device=srhd_awa::matrixgame::map_device;
'''
        if name.endswith('/MatrixEffectExplosion.cpp'):
            prelude+='#include "object_geometry.hpp"\n'
        if name.endswith(('/MatrixEffectPointLight.cpp','/MatrixEffectLandscapeSpot.cpp')):
            prelude+='''#ifdef _MSC_VER
#include <malloc.h>
#define effect_alloca _alloca
#else
#define effect_alloca __builtin_alloca
#endif
'''
        if name.endswith('/MatrixEffectLandscapeSpot.cpp'):
            prelude+='''#include <cstring>
static void effect_store_indices(std::uint16_t* destination,std::uint32_t packed) {
    std::memcpy(destination,&packed,sizeof(packed));
}
'''
        path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    # Full original Konus.cpp now owns its original statics. The earlier
    # selected defaults TU must not publish a second definition.
    name='MatrixGame/src/GameDefaultsNative.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    text,count=re.subn(r'^(?:::srhd_awa::matrixgame::buffers::VertexBuffer\*|int) CMatrixEffectKonus::m_VB(?:_ref)?;\n','',text,flags=re.M)
    if count!=2:raise ValueError('Original Konus static owner changed')
    path.write_bytes(text.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    # ClearEffects destroys real CDebris, whose base is the full original
    # animator. NativeVectorObjectAnim is a distinct earlier CPU-only type.
    source=(output/'MatrixLib/3G/VectorObject.cpp').read_text(encoding='utf-8')
    start=source.index('CVectorObjectAnim::CVectorObjectAnim()')
    end=source.index('CVectorObjectGroupUnit::CVectorObjectGroupUnit()',start)
    animation=source[start:end].replace('_alloca(','effect_alloca(')
    light_start=source.index('struct SColorInterval {')
    light_end=source.index('\n};',source.index('struct SLightData {',light_start))+len('\n};')
    light_data=source[light_start:light_end]
    name='MatrixLib/3G/OriginalVectorAnim.cpp';path=output/name
    prelude='''// Complete original animator methods, including billboard lights.
#include "Texture.hpp"
#include "VectorObject.hpp"
#include "CBillboard.hpp"
#ifdef _MSC_VER
#include <malloc.h>
#define effect_alloca _alloca
#else
#define effect_alloca __builtin_alloca
#endif
'''
    path.write_bytes((prelude+light_data+'\n'+animation).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes

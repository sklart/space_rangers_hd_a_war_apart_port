"""Original shadow extrusion and staging lifecycle, without a fake GPU."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt


def prepare_shadow_stencil(output):
    changes={};name='MatrixLib/3G/ShadowStencil.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    for axis in 'xyz':
        old='(*(std::uint32_t *)&vlightf.'+axis+')'
        if text.count(old)!=1:raise ValueError('Shadow light sign-bit expression changed')
        text=text.replace(old,'std::bit_cast<std::uint32_t>(vlightf.'+axis+')')
    text=text.replace('_alloca(', 'STENCIL_STACK_ALLOC(')
    text=adapt(text)
    text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,', 'stencil_device::SetTransform(stencil_device::Transform::World,')
    text=text.replace('g_D3DD->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,', 'stencil_device::DrawIndexedPrimitive(4u,')
    marker='void CVOShadowStencil::BeforeRenderAll()'
    if text.count(marker)!=1:raise ValueError('Shadow stencil entry changed')
    text=text.replace(marker,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''+marker,1)
    prelude='''#include "3g.hpp"
#include "map_device_backend.hpp"
#include <bit>
#ifdef _MSC_VER
#include <malloc.h>
#define STENCIL_STACK_ALLOC _alloca
#else
#define STENCIL_STACK_ALLOC __builtin_alloca
#endif
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace stencil_device=srhd_awa::matrixgame::map_device;
'''
    path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    name='MatrixLib/3G/ShadowStencil.hpp';path=output/name
    text=path.read_text(encoding='utf-8');marker='class CVOShadowStencil\n{'
    if text.count(marker)!=1:raise ValueError('Shadow stencil layout changed')
    text=text.replace(marker,marker+'\n    friend struct srhd_awa::matrixgame::ShadowStencilViewAccess;',1)
    marker='public:\n    static void StaticInit(void)'
    if text.count(marker)!=1:raise ValueError('Shadow stencil public boundary changed')
    text=text.replace(marker,'''public:
    // This original consumer already has friend access to VO geometry.
    static const SVOGeometry& ReadSourceGeometry(const CVectorObject& source) noexcept {
        return source.m_Geometry;
    }
    static void StaticInit(void)''',1)
    text='namespace srhd_awa::matrixgame {struct ShadowStencilViewAccess;}\n'+text
    path.write_bytes(text.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes

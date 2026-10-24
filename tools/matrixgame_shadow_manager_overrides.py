"""Compile the full original shadow manager through existing real adapters."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function


def prepare_shadow_manager(output):
    changed={};name='MatrixGame/src/MatrixShadowManager.hpp';path=output/name
    text=path.read_text(encoding='utf-8')
    text='#include "ShadowProj.hpp"\nclass CMatrixMapStatic;\nclass CVectorObjectAnim;\nclass CTexture;\n'+text
    path.write_bytes(text.encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    name='MatrixGame/src/MatrixShadowManager.cpp';path=output/name
    text=adapt(path.read_text(encoding='utf-8'))
    text=text.replace('D3DXVec3TransformNormal(', 'sm_geometry::TransformNormal(')
    text=text.replace('D3DXMatrixLookAtLH(', 'srhd_awa::matrixgame::geometry::LookAtLH(')
    text=text.replace('_alloca(', 'SHADOW_STACK_ALLOC(')
    text=text.replace('CACHE_CREATE_TEXTURE()', 'srhd_awa::matrixgame::legacy_texture::CreateUnmanaged()')
    text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,','sm_device::SetTransform(sm_device::Transform::World,')
    marker='CMatrixShadowProj::~CMatrixShadowProj()'
    if text.count(marker)!=1:raise ValueError('Original map-shadow destructor changed')
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
#include "ShadowProj.hpp"
#include "object_geometry.hpp"
#include "view_geometry.hpp"
#include "map_device_backend.hpp"
#include "unmanaged_texture_backend.hpp"
#ifdef _MSC_VER
#include <malloc.h>
#define SHADOW_STACK_ALLOC _alloca
#else
#define SHADOW_STACK_ALLOC __builtin_alloca
#endif
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace sm_geometry=srhd_awa::matrixgame::object_geometry;
namespace sm_device=srhd_awa::matrixgame::map_device;
'''
    path.write_bytes((prelude+text).encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    # Genuine downstream dependencies of ShadowProjBuildGeom. Keep the full
    # original algorithms, sharing the accepted real VO/animator declarations.
    source=(output/'MatrixLib/3G/VectorObject.cpp').read_text(encoding='utf-8')
    chunks=[]
    for signature in ['void CVectorObject::CalcShadowProjMatrix(int noframe,',
                      'void CVectorObject::CalcShadowProjMatrix(int cnt,']:
        body=function(source,signature)
        body=body.replace('D3DXMatrixLookAtLH(', 'srhd_awa::matrixgame::geometry::LookAtLH(')
        body=body.replace('D3DXMatrixMultiply(&ml2, wm + i, &ml);', 'ml2 = wm[i] * ml;')
        chunks.append(body)
    name='MatrixLib/3G/OriginalShadowProjection.cpp';path=output/name
    prelude='''// Original CVectorObject shadow-projection methods; no alternate geometry.
#include "VectorObject.hpp"
#include "ShadowProj.hpp"
#include "view_geometry.hpp"
'''
    path.write_bytes((prelude+'\n\n'.join(chunks)+'\n').encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changed

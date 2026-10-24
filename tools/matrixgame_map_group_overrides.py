"""Full map group TU and genuine map-owner group cleanup dependencies."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function


def prepare_map_group(output):
    changes={};name='MatrixGame/src/MatrixMapGroup.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    if text.count('CACHE_CREATE_TEXTUREMANAGED()')!=1:raise ValueError('Original group alpha factory changed')
    text=text.replace('CACHE_CREATE_TEXTUREMANAGED()','group_texture::CreateManaged()')
    text=adapt(text)
    for token,value in {'D3DFMT_A8':28,'D3DFMT_A8L8':51,'D3DFMT_A8R3G3B2':29,'D3DFMT_A8R8G8B8':21}.items():
        text=re.sub(r'\b'+token+r'\b',f'static_cast<TexturePixelFormat>({value}u)',text)
    text=text.replace('D3DLOCKED_RECT','group_texture::LockedRect').replace('D3D_OK','0').replace('D3DPT_TRIANGLELIST','4u')
    text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,','group_device::SetTransform(group_device::Transform::World,')
    text=text.replace('g_D3DD->DrawIndexedPrimitive(','group_device::DrawIndexedPrimitive(')
    if text.count('_alloca(')!=2:raise ValueError('Original group picking stack allocations changed')
    text=text.replace('_alloca(','group_alloca(')
    marker='static bool FreeResources('
    if text.count(marker)!=1:raise ValueError('Original group resource reminder changed')
    check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
    text=text.replace(marker,check+'\n'+marker)
    prelude='''#include "Texture.hpp"
#include "ShadowProj.hpp"
#include "render_pipeline_backend.hpp"
#include "renderer_ops.hpp"
#include "map_device_backend.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace group_texture=srhd_awa::matrixgame::legacy_texture;
namespace group_device=srhd_awa::matrixgame::map_device;
#ifdef _MSC_VER
#include <malloc.h>
#define group_alloca _alloca
#else
#define group_alloca __builtin_alloca
#endif
'''
    path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    # Retain the exact historical cleanup slice for source audits. The native
    # target now selects the complete OriginalMapPrepare.cpp instead; linking
    # both would duplicate these three genuine cleanup implementations.
    source=(output/'MatrixGame/src/MatrixMapPrepare.cpp').read_text(encoding='utf-8')
    bodies=[function(source,s) for s in ('void SGroupVisibility::Release(void)',
        'void CMatrixMap::ClearGroupVis(void)','void CMatrixMap::GroupClear(void)')]
    name='MatrixGame/src/OriginalGroupLifecycle.cpp';path=output/name
    path.write_bytes(('#include "Texture.hpp"\n#include "MatrixMap.hpp"\n'+'\n'.join(bodies)+'\n').encode())
    changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes

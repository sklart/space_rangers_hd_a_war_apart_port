"""Complete original terrain surface TU with typed existing device boundaries."""
import hashlib
import re
from matrixgame_vector_object_overrides import function


def prepare_terrain(output):
    header='MatrixGame/src/MatrixTerSurface.hpp';hp=output/header
    h=hp.read_text(encoding='utf-8');declaration='class CTerSurface : public Base::CMain {'
    if h.count(declaration)!=1:raise ValueError('Original terrain class changed')
    h=h.replace(declaration,'class CTextureManaged;\n'+declaration);hp.write_bytes(h.encode())
    name='MatrixGame/src/MatrixTerSurface.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    values={'D3DRS_ALPHABLENDENABLE':27,'D3DRS_ZWRITEENABLE':14,
            'D3DRS_FILLMODE':8,'D3DRS_TEXTUREFACTOR':60}
    for key,value in values.items():
        text=re.sub(r'\b'+key+r'\b','static_cast<srhd_awa::matrixgame::RasterRegister>('+str(value)+')',text)
    for key,value in [('TRUE','1u'),('FALSE','0u'),('D3DFILL_SOLID','3u'),('D3DPT_TRIANGLESTRIP','5u')]:
        text=re.sub(r'\b'+key+r'\b',value,text)
    text=text.replace('g_D3DD->SetFVF(','terrain_platform::ReadyDevice().SetFixedVertexLayout(')
    text=text.replace('g_D3DD->SetRenderState(','terrain_platform::ReadyDevice().SetRenderState(')
    text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,','terrain_device::SetTransform(terrain_device::Transform::World,')
    text=text.replace('g_D3DD->DrawIndexedPrimitive(','terrain_device::DrawIndexedPrimitive(')
    clear=function(text,'void CTerSurface::ClearSurfaces(void)')
    if clear.count('HFree(m_Surfaces, g_MatrixHeap)')!=1 or clear.count('HFree(m_SurfacesDraw, g_MatrixHeap)')!=1:
        raise ValueError('Original terrain ownership cleanup changed')
    end=clear.rfind('}')
    fixed=clear[:end]+'''    m_Surfaces = NULL;
    m_SurfacesDraw = NULL;
    m_SurfacesCnt = 0;
    m_SurfaceLeft = m_SurfaceRite = 0;
'''+clear[end:]
    text=text.replace(clear,fixed)
    prelude='''#include "Texture.hpp"
#include "map_device_backend.hpp"
namespace terrain_platform=srhd_awa::matrixgame::platform3g;
namespace terrain_device=srhd_awa::matrixgame::map_device;
'''
    # Other original statics are already owned by GameStaticResources.cpp.
    marker='CTerSurface **CTerSurface::m_SurfacesDraw;'
    if text.count(marker)!=1:raise ValueError('Original terrain static definitions changed')
    check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) terrain_platform::CheckDeviceResult((call), #call)
#endif
'''
    text=text.replace(marker,check+'\n'+marker)
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest(),header:hashlib.sha256(hp.read_bytes()).hexdigest()}

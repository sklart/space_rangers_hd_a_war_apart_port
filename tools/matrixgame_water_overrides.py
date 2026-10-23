"""Complete original water algorithm and genuine named SDK texture boundary."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt


def prepare_water(output):
    changes={};header='MatrixGame/src/MatrixWater.hpp';hp=output/header
    h=hp.read_text(encoding='utf-8');marker='struct SInshoreVertex {'
    if h.count(marker)!=1:raise ValueError('Original inshore vertex declaration changed')
    h=h.replace(marker,'class CTextureManaged;\n'+marker);hp.write_bytes(h.encode())
    changes[header]=hashlib.sha256(hp.read_bytes()).hexdigest()
    name='MatrixGame/src/MatrixWater.cpp';path=output/name;text=path.read_text(encoding='utf-8')
    get='(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, '
    if text.count(get)!=3:raise ValueError('Original water named texture requests changed')
    text=text.replace(get,'water_texture::GetManaged(')
    for texture in ('m_WaterTex1','m_WaterTex2'):
        call=texture+'->Tex()->SetPriority(0xFFFFFFFF);'
        if text.count(call)!=1:raise ValueError('Original water priority request changed')
        text=text.replace(call,'water_texture::SetPriority('+texture+'->Tex(),0xFFFFFFFFu);')
    text=adapt(text)
    text=text.replace('D3DPT_TRIANGLESTRIP','5u')
    text=text.replace('g_D3DD->SetTransform(D3DTS_WORLD,','water_device::SetTransform(water_device::Transform::World,')
    text=text.replace('g_D3DD->DrawPrimitive(','water_device::DrawPrimitive(')
    text=text.replace('g_D3DD->DrawIndexedPrimitive(','water_device::DrawIndexedPrimitive(')
    marker='void SInshorewave::Create('
    if text.count(marker)!=1:raise ValueError('Original water entry changed')
    check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
    text=text.replace(marker,check+'\n'+marker)
    prelude='''#include "Texture.hpp"
#include "map_device_backend.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace water_device=srhd_awa::matrixgame::map_device;
namespace water_texture=srhd_awa::matrixgame::legacy_texture;
'''
    path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes

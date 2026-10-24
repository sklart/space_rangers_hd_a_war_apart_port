"""Complete original UI batching renderer through existing native adapters."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt


def prepare_instant_draw(output):
    name = 'MatrixGame/src/MatrixInstantDraw.cpp'
    path = output / name
    text = adapt(path.read_text(encoding='utf-8'))
    marker = 'void CInstDraw::ActualDraw(void)'
    if text.count(marker) != 1:
        raise ValueError('Original UI batch renderer changed')
    text = text.replace('byte *verts;', 'std::uint8_t *verts;')
    text = text.replace('g_D3DD->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,',
                        'instant_device::DrawIndexedPrimitive(4u,')
    text = text.replace(marker, '''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
''' + marker, 1)
    prelude = '''#include "Texture.hpp"
#include "map_device_backend.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace instant_device=srhd_awa::matrixgame::map_device;
'''
    path.write_bytes((prelude + text).encode())
    return {name: hashlib.sha256(path.read_bytes()).hexdigest()}

"""Complete original camera and automatic battle-camera algorithms."""
import hashlib


def prepare_camera(output):
    name = 'MatrixGame/src/MatrixCamera.cpp'
    path = output / name
    text = path.read_text(encoding='utf-8')
    if text.count('void CMatrixCamera::RestoreCameraParams(void)') != 1:
        raise ValueError('Original camera restore changed')
    text = text.replace('D3DXMatrixRotationX(', 'srhd_awa::matrixgame::object_geometry::RotationX(')
    text = text.replace('D3DXMatrixRotationY(', 'srhd_awa::matrixgame::object_geometry::RotationY(')
    text = text.replace('D3DXMatrixTranslation(', 'srhd_awa::matrixgame::minimap_geometry::Translation(')
    text = text.replace('g_D3DD->SetViewport(', 'camera_offscreen::SetViewport(')
    for kind in ('WORLD', 'VIEW', 'PROJECTION'):
        text = text.replace('g_D3DD->SetTransform(D3DTS_'+kind+',',
                            'camera_device::SetTransform(camera_device::Transform::'+kind.title()+',')
    marker = 'void CMatrixCamera::RestoreCameraParams(void)'
    text = text.replace(marker, '''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) srhd_awa::matrixgame::platform3g::CheckDeviceResult((call), #call)
#endif
''' + marker, 1)
    data = ('''#include "Texture.hpp"
#include "object_geometry.hpp"
#include "minimap_geometry.hpp"
#include "offscreen_backend.hpp"
#include "map_device_backend.hpp"
namespace camera_offscreen=srhd_awa::matrixgame::offscreen;
namespace camera_device=srhd_awa::matrixgame::map_device;
''' + text).encode()
    path.write_bytes(data)
    return {name: hashlib.sha256(data).hexdigest()}

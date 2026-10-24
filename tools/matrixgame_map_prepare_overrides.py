"""Complete original map loading/restart/resource lifecycle, not a map stand-in."""
import hashlib,re

def prepare_map_prepare(output):
    text=(output/'MatrixGame/src/MatrixMapPrepare.cpp').read_text(encoding='utf-8')
    text=text.replace('CopyMemory(','std::memcpy(').replace('MoveMemory(','std::memmove(')
    text=re.sub(r'\bINT32\b','std::int32_t',text)
    replacements={
        'g_D3DD == NULL':'!prepare_device::DeviceAvailable()',
        'CDeviceState(g_D3DD)':'CDeviceState(prepare_device::StateBackend())',
        'HRESULT hr = g_D3DD->TestCooperativeLevel();':'auto hr = prepare_device::CooperativeLevel();',
        'g_D3DD->Reset(&g_D3Dpp)':'prepare_device::ResetCurrentPresentation()',
        'D3D_OK':'0','D3DERR_DEVICELOST':'static_cast<std::int32_t>(0x88760868u)',
        'D3DERR_DEVICENOTRESET':'static_cast<std::int32_t>(0x88760869u)',
        'D3DMATERIAL9':'srhd_awa::matrixgame::platform3g::VideoMaterial',
        'g_D3DD->SetMaterial(&mtrl)':'srhd_awa::matrixgame::platform3g::ReadyDevice().SetVideoMaterial(mtrl)',
        'g_D3DD->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL)':'srhd_awa::matrixgame::platform3g::ReadyDevice().SetRenderState(static_cast<srhd_awa::matrixgame::RasterRegister>(146),0u)',
        'D3DXMatrixRotationX(':'prepare_geometry::RotationX(',
        'D3DXVec3TransformNormal(':'prepare_geometry::TransformNormal(',
        '_alloca(':'MATRIXGAME_LOGIC_STACK_ALLOC(',
    }
    for old,new in replacements.items():
        if old not in text:raise ValueError('Original map preparation boundary changed: '+old)
        text=text.replace(old,new)
    prelude='''#include "Texture.hpp"
#include "map_device_backend.hpp"
#include "DeviceState.hpp"
#include "map_logic_backend.hpp"
#include "object_geometry.hpp"
#include <cstring>
namespace prepare_device=srhd_awa::matrixgame::map_device;
namespace prepare_geometry=srhd_awa::matrixgame::object_geometry;
'''
    name='MatrixGame/src/OriginalMapPrepare.cpp';path=output/name
    path.write_bytes((prelude+text).encode());return {name:hashlib.sha256(path.read_bytes()).hexdigest()}

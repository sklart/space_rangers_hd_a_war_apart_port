"""Complete original vector group block, including load, links, bounds and rendering."""
import hashlib
import re


def prepare_vector_group(output):
    source=(output/'MatrixLib/3G/VectorObject.cpp').read_text(encoding='utf-8')
    marker='CVectorObjectGroupUnit::CVectorObjectGroupUnit()'
    if source.count(marker)!=1:raise ValueError('Original vector group boundary changed')
    block=source[source.index(marker):]
    if block.count('void CVectorObjectGroup::ShadowStencil_DX_Free(void)')!=1:
        raise ValueError('Original vector group end changed')
    block=block.replace('D3DXVec3TransformNormal(','group_geometry::TransformNormal(')
    block=block.replace('g_D3DD->SetTransform(D3DTS_WORLD,','group_device::SetTransform(group_device::Transform::World,')
    replacements={
        'std::uint32_t data;':'uintptr_t data;',
        'EnumFrameVerts_transform_position(ENUM_VERTS_HANDLER evh, std::uint32_t data)':'EnumFrameVerts_transform_position(ENUM_VERTS_HANDLER evh, uintptr_t data)',
        'void CVectorObjectGroup::Draw(std::uint32_t userparam)':'void CVectorObjectGroup::Draw(uintptr_t userparam)',
    }
    for before,after in replacements.items():
        if block.count(before)!=1:raise ValueError('Original vector group opaque payload changed: '+before)
        block=block.replace(before,after)
    prelude='''// Complete contiguous original vector-group implementation.
#include "Texture.hpp"
#include "VectorObject.hpp"
#include "CBillboard.hpp"
#include "ShadowStencil.hpp"
#include "object_geometry.hpp"
#include "map_device_backend.hpp"
namespace group_geometry=srhd_awa::matrixgame::object_geometry;
namespace group_device=srhd_awa::matrixgame::map_device;
'''
    name='MatrixLib/3G/OriginalVectorGroup.cpp';path=output/name
    path.write_bytes((prelude+block).encode())
    changes={name:hashlib.sha256(path.read_bytes()).hexdigest()}
    name='MatrixLib/3G/VectorObject.hpp';path=output/name
    header=path.read_text(encoding='utf-8')
    header,count=re.subn(r'void Draw\(std::uint32_t user_param\)','void Draw(uintptr_t user_param)',header)
    if count!=2:raise ValueError('Original animator/group draw declarations changed')
    before='EnumFrameVerts_transform_position(ENUM_VERTS_HANDLER evh, std::uint32_t data)'
    if header.count(before)!=1:raise ValueError('Original group enumeration declaration changed')
    header=header.replace(before,'EnumFrameVerts_transform_position(ENUM_VERTS_HANDLER evh, uintptr_t data)')
    path.write_bytes(header.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes

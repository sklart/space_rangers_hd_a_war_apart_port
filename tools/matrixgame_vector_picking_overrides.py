"""Original VO picking algorithms, with typed math and bit-preserving reads."""
import hashlib
import re
from matrixgame_vector_object_overrides import function


def prepare_vector_picking(output):
    source = (output / 'MatrixLib/3G/VectorObject.cpp').read_text(encoding='utf-8')
    chunks = []
    for signature in ['bool CVectorObject::PickSimple(', 'bool CVectorObject::Pick(',
                      'bool CVectorObject::PickFull(']:
        text = function(source, signature)
        text = text.replace('D3DXVec3TransformNormal(', 'vo_geometry::TransformNormal(')
        text = text.replace('D3DXBoxBoundProbe(', 'vo_picking::BoxBoundProbe(')
        text = text.replace('_alloca(', 'VO_PICK_STACK_ALLOC(')
        for i, axis in enumerate('xyz'):
            old = f'*(((DWORD *)&adir) + {i}) = *(((DWORD *)&_dir) + {i}) & 0x7FFFFFFF;'
            text = text.replace(old, f'adir.{axis} = std::bit_cast<float>(std::bit_cast<std::uint32_t>(_dir.{axis}) & 0x7FFFFFFF);')
        text = re.sub(r'\*\(\(DWORD \*\)&coord\[([01])\]\)',
                      r'std::bit_cast<std::uint32_t>(coord[\1])', text)
        text = re.sub(r'\bDWORD\b', 'std::uint32_t', text)
        text = re.sub(r'\bBYTE\b', 'std::uint8_t', text)
        # The redundant self-assignment in the original initializer adds no
        # dependency; retain the same two component expressions and order.
        text = text.replace('{k[0] = -_dir.x * kk, k[1] = -_dir.z * kk}', '{-_dir.x * kk, -_dir.z * kk}')
        text = text.replace('{k[0] = -_dir.x * kk, -_dir.y * kk}', '{-_dir.x * kk, -_dir.y * kk}')
        chunks.append(text)
    prelude = '''// Original CVectorObject picking, including original masks and filtering.
#include "3g.hpp"
#include "VectorObject.hpp"
#include "Math3D.hpp"
#include "object_geometry.hpp"
#include "vector_picking.hpp"
#include <bit>
#include <cstdint>
#ifdef _MSC_VER
#include <malloc.h>
#define VO_PICK_STACK_ALLOC _alloca
#else
#define VO_PICK_STACK_ALLOC __builtin_alloca
#endif
namespace vo_geometry=srhd_awa::matrixgame::object_geometry;
namespace vo_picking=srhd_awa::matrixgame::vector_picking;
'''
    name = 'MatrixLib/3G/OriginalVectorPicking.cpp'
    path = output / name
    path.write_bytes((prelude + '\n\n'.join(chunks) + '\n').encode())
    return {name: hashlib.sha256(path.read_bytes()).hexdigest()}

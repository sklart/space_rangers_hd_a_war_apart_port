// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "math_types.hpp"
#include <cstddef>
#include <cstring>
namespace srhd_awa::matrixgame::geometry {
// Original D3DX strided coordinate-array contract. Copy only XYZ; interleaved
// color/UV fields belong to the caller. Scalar math uses genuine DirectXMath.
inline D3DXVECTOR3* TransformCoordArray(D3DXVECTOR3* output,std::uint32_t output_stride,
    const D3DXVECTOR3* input,std::uint32_t input_stride,const D3DXMATRIX* matrix,std::uint32_t count) {
    auto* dest=reinterpret_cast<std::byte*>(output);
    const auto* source=reinterpret_cast<const std::byte*>(input);
    for(std::uint32_t i=0;i<count;++i) {
        D3DXVECTOR3 value,result;
        std::memcpy(&value,source+std::size_t(i)*input_stride,sizeof(value));
        D3DXVec3TransformCoord(&result,&value,matrix);
        std::memcpy(dest+std::size_t(i)*output_stride,&result,sizeof(result));
    }
    return output;
}
}

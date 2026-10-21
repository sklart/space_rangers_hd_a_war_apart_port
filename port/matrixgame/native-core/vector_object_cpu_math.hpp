// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "math_types.hpp"
// Actual CPU coordinate transform, no renderer/device interface. The original
// GetBound uses an in-place strided array of eight corners.
inline D3DXVECTOR3* D3DXVec3TransformCoordArray(D3DXVECTOR3* out, std::uint32_t out_stride,
    const D3DXVECTOR3* in, std::uint32_t in_stride, const D3DXMATRIX* matrix, std::uint32_t count) {
    const auto m = DirectX::XMLoadFloat4x4(matrix);
    auto* destination = reinterpret_cast<std::byte*>(out);
    const auto* source = reinterpret_cast<const std::byte*>(in);
    for (std::uint32_t i=0;i<count;++i) {
        D3DXVECTOR3 value, transformed;
        std::memcpy(&value,source+std::size_t(i)*in_stride,sizeof(value));
        StoreVector3(&transformed, DirectX::XMVector3TransformCoord(LoadVector3(&value),m));
        std::memcpy(destination+std::size_t(i)*out_stride,&transformed,sizeof(transformed));
    }
    return out;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "math_types.hpp"
#include <cstddef>
#include <cstring>
namespace srhd_awa::matrixgame::minimap_geometry {
inline D3DXMATRIX* Translation(D3DXMATRIX* out,float x,float y,float z) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixTranslation(x,y,z));return out;
}
inline D3DXMATRIX* OrthoLH(D3DXMATRIX* out,float w,float h,float near_z,float far_z) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixOrthographicLH(w,h,near_z,far_z));return out;
}
inline D3DXVECTOR2* TransformCoord(D3DXVECTOR2* out,const D3DXVECTOR2* in,const D3DXMATRIX* matrix) {
    const auto value=DirectX::XMVectorSet(in->x,in->y,0,0);
    DirectX::XMFLOAT2 result;
    DirectX::XMStoreFloat2(&result,DirectX::XMVector2TransformCoord(value,DirectX::XMLoadFloat4x4(matrix)));
    out->x=result.x;out->y=result.y;return out;
}
inline D3DXVECTOR2* TransformCoordArray(D3DXVECTOR2* out,std::uint32_t out_stride,
    const D3DXVECTOR2* in,std::uint32_t in_stride,const D3DXMATRIX* matrix,std::uint32_t count) {
    auto* dest=reinterpret_cast<std::byte*>(out);
    const auto* source=reinterpret_cast<const std::byte*>(in);
    for(std::uint32_t i=0;i<count;++i) {
        D3DXVECTOR2 value,result;
        std::memcpy(&value,source+std::size_t(i)*in_stride,sizeof(value));
        TransformCoord(&result,&value,matrix);
        std::memcpy(dest+std::size_t(i)*out_stride,&result,sizeof(result));
    }
    return out;
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "object_geometry.hpp"
namespace srhd_awa::matrixgame::actor_geometry {
inline float Dot2(const D3DXVECTOR2* a,const D3DXVECTOR2* b) {
    return DirectX::XMVectorGetX(DirectX::XMVector2Dot(LoadVector2(a),LoadVector2(b)));
}
inline D3DXVECTOR2* TransformCoord2(D3DXVECTOR2* out,const D3DXVECTOR2* in,const D3DXMATRIX* matrix) {
    const auto value=DirectX::XMVector2TransformCoord(LoadVector2(in),DirectX::XMLoadFloat4x4(matrix));
    out->x=DirectX::XMVectorGetX(value);out->y=DirectX::XMVectorGetY(value);return out;
}
inline D3DXPLANE* TransformPlane(D3DXPLANE* out,const D3DXPLANE* in,const D3DXMATRIX* matrix) {
    const auto value=DirectX::XMPlaneTransform(DirectX::XMVectorSet(in->a,in->b,in->c,in->d),DirectX::XMLoadFloat4x4(matrix));
    return StorePlane(out,value);
}
}

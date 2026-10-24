// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "math_types.hpp"
namespace srhd_awa::matrixgame::object_geometry {
inline D3DXMATRIX* RotationYawPitchRoll(D3DXMATRIX* out,float yaw,float pitch,float roll) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixRotationRollPitchYaw(pitch,yaw,roll));return out;
}
inline D3DXMATRIX* RotationX(D3DXMATRIX* out,float angle) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixRotationX(angle));return out;
}
inline D3DXMATRIX* RotationY(D3DXMATRIX* out,float angle) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixRotationY(angle));return out;
}
inline D3DXMATRIX* Scaling(D3DXMATRIX* out,float x,float y,float z) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixScaling(x,y,z));return out;
}
inline D3DXVECTOR3* TransformNormal(D3DXVECTOR3* out,const D3DXVECTOR3* in,const D3DXMATRIX* matrix) {
    return StoreVector3(out,DirectX::XMVector3TransformNormal(LoadVector3(in),DirectX::XMLoadFloat4x4(matrix)));
}
}

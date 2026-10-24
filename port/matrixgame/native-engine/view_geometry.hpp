// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "math_types.hpp"
namespace srhd_awa::matrixgame::geometry {
inline D3DXMATRIX* LookAtLH(D3DXMATRIX* output,const D3DXVECTOR3* eye,
    const D3DXVECTOR3* target,const D3DXVECTOR3* up) {
    DirectX::XMStoreFloat4x4(output,DirectX::XMMatrixLookAtLH(LoadVector3(eye),LoadVector3(target),LoadVector3(up)));
    return output;
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "offscreen_backend.hpp"
namespace srhd_awa::matrixgame::shadow_texture {
// Preserve the original D3DX size/format negotiation using the real SDK.
platform3g::DeviceResult CreateRenderTexture(std::uint32_t,std::uint32_t,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
// Upload the genuinely selected CPU staging buffers to owned SDK VB/IB.
// This first integration deliberately retains no cross-draw GPU cache.
platform3g::DeviceResult DrawIndexedStaging(std::uint32_t,int,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t);
}

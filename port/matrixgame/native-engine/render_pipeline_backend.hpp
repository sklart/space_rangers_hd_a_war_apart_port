// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "math_types.hpp"
#include <cstring>
namespace srhd_awa::matrixgame::render_pipeline {
inline std::uint32_t MaxSimultaneousTextures() {
    (void)platform3g::ReadyDevice();
    return platform3g::Current().caps.max_simultaneous_textures;
}
inline platform3g::DeviceResult TextureTransform(std::uint32_t stage,const D3DXMATRIX* matrix) {
    platform3g::WorldMatrix value;static_assert(sizeof(value)==sizeof(*matrix));
    std::memcpy(value.data(),matrix,sizeof(value));
    return platform3g::ReadyDevice().SetTextureMatrix(stage,value);
}
}

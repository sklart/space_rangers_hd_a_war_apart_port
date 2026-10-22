// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../native-engine/cache_resources.hpp"
#include "mesh_renderer.hpp"
namespace srhd_awa::matrixgame {
// Holds the cache-owned payload across the existing real upload. Unknown or
// destroyed material owners throw before any GPU resource is replaced.
inline void UploadCachedMaterialTexture(DekoMeshRenderer& renderer,std::uint32_t slot,
    const MaterialTexture& material) {
    const auto lease=material.Read();
    renderer.SetTextureAsset(slot,*lease);
}
}

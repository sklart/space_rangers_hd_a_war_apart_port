// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "init_runtime.hpp"
#include "VectorObjectCpu.hpp"
#include "../deko/vector_submission.hpp"

namespace srhd_awa::matrixgame {
// Each surface must receive an explicit material. This is the existing native
// single-pass shader contract, not CSkinManager's multi-pass effects/lighting.
struct CachedFrameMaterial {
    MaterialTexture texture;
    VectorMaterial state;
};
struct CachedFrame {
    std::vector<MeshSubmission> submissions;
    // Retain CPU texture payload through the complete upload/draw operation.
    std::vector<std::shared_ptr<const TextureAsset>> textures;
};
CachedFrame PrepareCachedFrame(NativeInitRuntime&, CVectorObject&, int frame,
    std::span<const CachedFrameMaterial>);
// The CVectorObject reference is borrowed and must still be a live cache entry.
// Ranges address the current buffer bindings: submit them before another frame
// changes those bindings. NativeDekoSession::Draw enforces this ordering.
}

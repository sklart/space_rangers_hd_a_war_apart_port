// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "mesh_renderer.hpp"
#include <string_view>
namespace srhd_awa::matrixgame {
struct PngTextureSize {std::uint32_t width,height;};
// Reads through the native CFile/PKG layer, preserves straight RGBA8 and throws
// on missing/corrupt input. No white fallback, color key or alpha flattening.
PngTextureSize UploadPngTexture(DekoMeshRenderer&,std::uint32_t slot,std::wstring_view path);
}

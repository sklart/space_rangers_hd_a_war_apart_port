// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
namespace srhd_awa::matrixgame {
// D3D9 samples at integer screen coordinates. The native rasterizer samples at
// half integers. Move the physical viewport by half a pixel in top-left space;
// original viewport queries, projection matrices, XYZRHW and clear rectangles
// retain their original logical values. Applies to both world and UI geometry.
inline std::array<float,4> LegacyRasterViewport(std::array<float,4> logical)noexcept{
    logical[0]+=.5f;logical[1]+=.5f;return logical;
}
// The physical half-pixel shift must not expand D3D's logical viewport clip.
// Input is the validated, integral FixedVertexState viewport, before shifting.
// This is implicit viewport clipping, independent of D3DRS_SCISSORTESTENABLE.
inline std::array<unsigned,4> LegacyRasterClip(const std::array<float,4>& logical)noexcept{
    return {unsigned(logical[0]),unsigned(logical[1]),unsigned(logical[2]),unsigned(logical[3])};
}
}

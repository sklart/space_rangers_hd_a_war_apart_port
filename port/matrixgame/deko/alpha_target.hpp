// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "raster_state.hpp"
namespace srhd_awa::matrixgame {
// A8 alpha is physically R8. Keep logical source alpha for alpha test, but
// route destination alpha and the alpha-channel saturate factor to R blending.
inline RasterBlendFactor AlphaTargetFactor(RasterBlendFactor f)noexcept{
 switch(f){case RasterBlendFactor::DestinationAlpha:return RasterBlendFactor::DestinationColor;
 case RasterBlendFactor::InverseDestinationAlpha:return RasterBlendFactor::InverseDestinationColor;
 case RasterBlendFactor::SourceAlphaSaturate:return RasterBlendFactor::One;default:return f;}
}
inline MeshRasterState AlphaTargetRaster(MeshRasterState r){r.Validate();r.color_write=(r.color_write&8u)?1u:0u;r.source=AlphaTargetFactor(r.source);r.destination=AlphaTargetFactor(r.destination);return r;}
}

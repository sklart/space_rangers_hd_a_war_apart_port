// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "material_combiner.hpp"
#include <algorithm>
#include <cmath>
#include <span>
namespace srhd_awa::matrixgame::materials {
// Serialized D3D9 raster capability IDs, describing implemented shader paths.
// No vertex/range fog capability is advertised by this pixel-only backend.
inline constexpr std::uint32_t PixelFogCaps=0x00000100u|0x00100000u|0x00200000u;
struct alignas(16) PixelFogUniforms {
    std::array<std::uint32_t,4> controls{}; // enabled, formula(EXP/EXP2/LINEAR), W depth, reserved
    Color parameters{0,1,1,0}; // start,end,density,reserved
    Color color{}; // alpha is retained metadata, never used for fog blending
};
static_assert(sizeof(PixelFogUniforms)==48 && offsetof(PixelFogUniforms,parameters)==16 && offsetof(PixelFogUniforms,color)==32);
inline PixelFogUniforms PreparePixelFog(bool enabled,std::uint32_t mode,float start,float end,float density,std::uint32_t argb,std::span<const float,16> projection){
    PixelFogUniforms out;if(!enabled)return out;
    if(mode<1 || mode>3)throw Incomplete("native vertex/manual fog requires a separate backend; no pixel fog mode");
    if(mode==3 && (!std::isfinite(start) || !std::isfinite(end) || end<=start))throw Incomplete("native linear fog requires finite increasing start/end");
    if(mode!=3 && (!std::isfinite(density) || density<0 || density>1))throw Incomplete("native exponential fog density outside [0,1]");
    // D3D selects Z fog for affine projections and W fog for compliant
    // perspective projections, including when vertices arrive as XYZRHW.
    const bool affine=projection[3]==0 && projection[7]==0 && projection[11]==0 && projection[15]==1;
    const bool perspective=projection[3]==0 && projection[7]==0 && projection[11]==1 && projection[15]==0;
    if(!affine && !perspective)throw Incomplete("native pixel fog requires canonical affine/perspective projection");
    out.controls={1,mode,perspective?1u:0u,0};out.parameters={start,end,density,0};
    out.color={float((argb>>16)&255)/255,float((argb>>8)&255)/255,float(argb&255)/255,float(argb>>24)/255};return out;
}
inline float PixelFogFactor(const PixelFogUniforms& u,float depth){
    if(!u.controls[0])return 1;
    if(!std::isfinite(depth))throw std::invalid_argument("nonfinite pixel fog depth");
    float f;
    switch(u.controls[1]){
    case 1:f=std::exp(-u.parameters[2]*depth);break;
    case 2:{const auto d=u.parameters[2]*depth;f=std::exp(-d*d);break;}
    case 3:f=(u.parameters[1]-depth)/(u.parameters[1]-u.parameters[0]);break;
    default:throw Incomplete("unprepared native pixel fog formula");
    }
    return std::clamp(f,0.f,1.f);
}
inline Color ApplyPixelFog(const PixelFogUniforms& u,Color c,float depth){
    if(u.controls[0]){const auto f=PixelFogFactor(u,depth);for(unsigned i=0;i<3;++i)c[i]=c[i]*f+u.color[i]*(1-f);}return c;
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
namespace srhd_awa::matrixgame::diagnostic {
// Diagnostic data only. This rectangle is never labelled a MatrixGame frame.
inline constexpr std::array<std::byte,8> SmokeBC1{
    std::byte{0x00},std::byte{0xf8},std::byte{0xe0},std::byte{0x07},
    std::byte{0},std::byte{0},std::byte{0},std::byte{0}};
struct SmokeVertex {float x,y,z,rhw;std::uint32_t diffuse;float u,v;};
static_assert(sizeof(SmokeVertex)==28 && offsetof(SmokeVertex,diffuse)==16 && offsetof(SmokeVertex,u)==20);
inline std::array<SmokeVertex,4> SmokeQuad(unsigned width,unsigned height){
    if(width<8 || height<8 || width>16384 || height>16384)throw std::invalid_argument("diagnostic extent");
    const float left=float(width/4)-.5f,right=float(3*width/4)-.5f,top=float(height/3)-.5f,bottom=float(2*height/3)-.5f;
    return {{{left,top,0,1,0xffffffff,0,0},{left,bottom,0,1,0xffffffff,0,1},
             {right,top,0,1,0xffffffff,1,0},{right,bottom,0,1,0xffffffff,1,1}}};
}
inline void ValidateFrame(unsigned width,unsigned height,std::span<const std::byte> bytes){
    if(!width || !height || width>16384 || height>16384 || std::uint64_t(width)*height*4!=bytes.size())
        throw std::invalid_argument("diagnostic BGRA framebuffer size/format");
}
inline void CheckSmokePixels(unsigned width,unsigned height,std::span<const std::byte> bgra){
    (void)SmokeQuad(width,height);ValidateFrame(width,height,bgra);
    const auto center=(std::size_t(height/2)*width+width/2)*4;
    if(bgra[center]!=std::byte{0} || bgra[center+1]!=std::byte{0} || bgra[center+2]!=std::byte{255} || bgra[center+3]!=std::byte{255})
        throw std::runtime_error("BC1 smoke framebuffer center is not opaque red");
}
inline void CheckIdentityPresentation(unsigned width,unsigned height,std::span<const std::byte> source,std::span<const std::byte> output){
    ValidateFrame(width,height,source);ValidateFrame(width,height,output);
    for(std::size_t i=0;i<source.size();++i){
        const auto a=std::to_integer<int>(source[i]),b=std::to_integer<int>(output[i]);
        const int tolerance=i%4==3?0:1;
        if(b<a-tolerance || b>a+tolerance)throw std::runtime_error("identity presentation differs from source (orientation/channels)");
    }
}
inline platform3g::GammaRamp SmokeGammaRamp()noexcept {
    platform3g::GammaRamp ramp;
    ramp.red.fill(16384);ramp.green.fill(32768);ramp.blue.fill(49152);return ramp;
}
inline void CheckGammaSmokePixels(unsigned width,unsigned height,std::span<const std::byte> bgra){
    (void)SmokeQuad(width,height);ValidateFrame(width,height,bgra);
    const auto center=(std::size_t(height/2)*width+width/2)*4;
    const std::array<int,4> expected{191,128,64,255};
    for(unsigned c=0;c<4;++c){const int value=std::to_integer<unsigned char>(bgra[center+c]);
        if(value<expected[c]-(c==3?0:1) || value>expected[c]+(c==3?0:1))throw std::runtime_error("gamma smoke presentation pixel does not match WORD ramp");}
}
}

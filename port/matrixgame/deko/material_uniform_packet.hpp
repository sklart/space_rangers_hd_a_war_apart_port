// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "material_combiner.hpp"
#include "fixed_vertex.hpp"
#include "pixel_fog.hpp"
#include <deko3d.h>
#include <algorithm>
#include <cstring>
#include <span>
namespace srhd_awa::matrixgame::materials::uniform_packet {
inline constexpr unsigned FogOffset=512,RasterOffset=768,VertexOffset=1024,Bytes=DK_MEMBLOCK_ALIGNMENT;
inline constexpr unsigned MinimumBytes=(VertexOffset+sizeof(VertexUniforms)+DK_UNIFORM_BUF_ALIGNMENT-1)&~(DK_UNIFORM_BUF_ALIGNMENT-1);
static_assert(sizeof(Uniforms)<=FogOffset);
static_assert(FogOffset+sizeof(PixelFogUniforms)<=RasterOffset);
static_assert(RasterOffset+16<=VertexOffset && VertexOffset+sizeof(VertexUniforms)<=Bytes);
static_assert(FogOffset%DK_UNIFORM_BUF_ALIGNMENT==0 && RasterOffset%DK_UNIFORM_BUF_ALIGNMENT==0 && VertexOffset%DK_UNIFORM_BUF_ALIGNMENT==0);
inline void Write(std::span<std::byte> output,const Uniforms& material,const VertexUniforms* vertex,const PixelFogUniforms& fog,unsigned shade){
    if(output.size()!=MinimumBytes && output.size()<Bytes)throw std::length_error("material uniform packet storage");
    if(shade!=1 && shade!=2)throw std::invalid_argument("material uniform packet shading");
    // Write each byte once: preserve the fully-zeroed padding contract while
    // avoiding a whole-packet clear followed by overwriting all payloads.
    std::memset(output.data()+sizeof(Uniforms),0,FogOffset-sizeof(Uniforms));
    std::memset(output.data()+FogOffset+sizeof(PixelFogUniforms),0,RasterOffset-FogOffset-sizeof(PixelFogUniforms));
    std::memset(output.data()+RasterOffset+16,0,VertexOffset-RasterOffset-16);
    const auto bytes=std::min<std::size_t>(output.size(),Bytes);
    if(vertex)std::memset(output.data()+VertexOffset+sizeof(VertexUniforms),0,bytes-VertexOffset-sizeof(VertexUniforms));
    else std::memset(output.data()+VertexOffset,0,bytes-VertexOffset);
    std::memcpy(output.data(),&material,sizeof(material));
    std::memcpy(output.data()+FogOffset,&fog,sizeof(fog));
    const std::array<std::uint32_t,4> controls{shade,0,0,0};
    std::memcpy(output.data()+RasterOffset,controls.data(),sizeof(controls));
    if(vertex)std::memcpy(output.data()+VertexOffset,vertex,sizeof(*vertex));
}
}

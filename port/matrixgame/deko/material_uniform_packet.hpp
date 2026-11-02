// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "material_combiner.hpp"
#include "fixed_vertex.hpp"
#include "pixel_fog.hpp"
#include <deko3d.h>
#include <cstring>
#include <span>
namespace srhd_awa::matrixgame::materials::uniform_packet {
inline constexpr unsigned FogOffset=512,RasterOffset=768,VertexOffset=1024,Bytes=DK_MEMBLOCK_ALIGNMENT;
static_assert(sizeof(Uniforms)<=FogOffset);
static_assert(FogOffset+sizeof(PixelFogUniforms)<=RasterOffset);
static_assert(RasterOffset+16<=VertexOffset && VertexOffset+sizeof(VertexUniforms)<=Bytes);
static_assert(FogOffset%DK_UNIFORM_BUF_ALIGNMENT==0 && RasterOffset%DK_UNIFORM_BUF_ALIGNMENT==0 && VertexOffset%DK_UNIFORM_BUF_ALIGNMENT==0);
inline void Write(std::span<std::byte> output,const Uniforms& material,const VertexUniforms* vertex,const PixelFogUniforms& fog,unsigned shade){
    if(output.size()<Bytes)throw std::length_error("material uniform packet storage");
    if(shade!=1 && shade!=2)throw std::invalid_argument("material uniform packet shading");
    std::memset(output.data(),0,Bytes);
    std::memcpy(output.data(),&material,sizeof(material));
    std::memcpy(output.data()+FogOffset,&fog,sizeof(fog));
    const std::array<std::uint32_t,4> controls{shade,0,0,0};
    std::memcpy(output.data()+RasterOffset,controls.data(),sizeof(controls));
    if(vertex)std::memcpy(output.data()+VertexOffset,vertex,sizeof(*vertex));
}
}

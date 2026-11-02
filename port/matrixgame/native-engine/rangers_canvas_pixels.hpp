// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
namespace srhd_awa::matrixgame::rangers_canvas {
struct Plan {int width,height,row_bytes;std::size_t source_bytes;int x,y,draw_width,draw_height;};
inline Plan Layout(int pitch,int width,int height,int output_width,int output_height){
 if(width<=0 || height<=0 || width>16384 || height>16384 || pitch<width*2 ||
    output_width<=0 || output_height<=0 || output_width>16384 || output_height>16384)
  throw std::invalid_argument("native Rangers canvas dimensions/pitch");
 const auto bytes=std::uint64_t(pitch)*(height-1)+std::uint64_t(width)*2;
 if(bytes>std::size_t(-1))throw std::invalid_argument("native Rangers canvas span overflow");
 int dw,dh;
 if(std::int64_t(output_width)*height<=std::int64_t(output_height)*width){dw=output_width;dh=int(std::int64_t(height)*dw/width);}
 else{dh=output_height;dw=int(std::int64_t(width)*dh/height);}
 if(!dw || !dh)throw std::invalid_argument("native Rangers canvas destination collapsed");
 return {width,height,width*2,std::size_t(bytes),(output_width-dw)/2,(output_height-dh)/2,dw,dh};
}
// Borrowed source and texture lock are distinct buffers. Preserve the RGB565
// words exactly; neither source padding nor destination padding is an image row.
inline void CopyRows(const Plan& plan,std::span<const std::byte> source,int source_pitch,
                     std::span<std::byte> destination,int destination_pitch){
 const auto expected=Layout(source_pitch,plan.width,plan.height,plan.width,plan.height);
 if(plan.row_bytes!=expected.row_bytes || plan.source_bytes!=expected.source_bytes || destination_pitch<plan.row_bytes || source.size()<expected.source_bytes ||
    destination.size()<std::uint64_t(destination_pitch)*(plan.height-1)+plan.row_bytes)
  throw std::invalid_argument("native Rangers canvas truncated rows");
 for(int y=0;y<plan.height;++y)std::memcpy(destination.data()+std::size_t(y)*destination_pitch,
   source.data()+std::size_t(y)*source_pitch,std::size_t(plan.row_bytes));
}
struct Vertex {float x,y,z,rhw;std::uint32_t diffuse;float u,v;};
static_assert(sizeof(Vertex)==28 && offsetof(Vertex,u)==20);
}

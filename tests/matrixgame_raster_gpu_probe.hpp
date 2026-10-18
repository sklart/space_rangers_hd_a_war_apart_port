// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../port/matrixgame/deko/mesh_renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace srhd_awa::matrixgame {
// Controlled actual-GPU checks. This helper is never a host framebuffer renderer.
inline void ProbeRasterGpu(DekoMeshRenderer& renderer) {
  const std::array quad{
    MeshVertex{{-1,1,0},{0,0,1},{0,0}},MeshVertex{{1,1,0},{0,0,1},{1,0}},
    MeshVertex{{-1,-1,0},{0,0,1},{0,1}},MeshVertex{{1,-1,0},{0,0,1},{1,1}}};
  const std::array<std::uint16_t,6> indices{0,1,2,2,1,3};renderer.SetMesh(quad,indices);
  for(unsigned alpha:{7u,8u,128u}) {
    const std::array<std::byte,4> pixel{std::byte{255},std::byte{255},std::byte{255},std::byte(alpha)};
    renderer.SetTextureRGBA8(alpha==7?1:alpha==8?2:3,1,1,4,pixel);
  }
  auto submission=[](float depth,std::array<float,4> color,std::uint32_t texture=0) {
    MeshSubmission out;out.range={0,0,0,6,0,4};out.texture_slot=texture;out.draw.diffuse=color;
    for(unsigned i=0;i<4;++i)out.draw.clip_from_model[i*5]=out.draw.normal_from_model[i*5]=1;
    out.draw.clip_from_model[14]=depth;out.draw.light[3]=1;return out;
  };
  const auto red=submission(.75f,{1,0,0,1}),blue=submission(.75f,{0,0,1,1});
  const auto check=[&](std::span<const MeshSubmission> draws,std::array<int,4> expected,int tolerance=0) {
    std::vector<std::byte> pixels(64*64*4);renderer.DrawFrame(draws,pixels);
    for(unsigned i=0;i<64*64;++i)for(unsigned c=0;c<4;++c)
      if(std::abs(int(std::to_integer<unsigned>(pixels[i*4+c]))-expected[c])>tolerance)
        throw std::runtime_error("GPU raster state pixel mismatch");
  };
  auto front=submission(.25f,{0,1,0,1},1);front.raster.alpha_test=true;
  std::array pair{front,red};check(pair,{255,0,0,255}); // alpha 7 fails >=8 and writes no depth
  front.texture_slot=2;pair={front,red};check(pair,{0,255,0,8}); // equality passes
  front.raster.depth_write=false;pair={front,red};check(pair,{255,0,0,255});
  front=submission(.25f,{1,0,0,1},3);front.raster.blend_enable=true;front.raster.depth_write=false;
  pair={blue,front};check(pair,{128,0,127,191},1); // shared D3D9 RGB/alpha factors
  front.raster.source=RasterBlendFactor::One;front.raster.destination=RasterBlendFactor::One;
  pair={blue,front};check(pair,{255,0,255,255});
  front.raster.color_write=1;front.draw.diffuse={0,1,0,1};pair={blue,front};check(pair,{0,0,255,255});
  auto green=submission(.5f,{0,1,0,1});
  const std::array triple{blue,front,green};check(triple,{0,255,0,255});
  // The next frame must restore opaque state even after additive/masked draws.
  pair={blue,green};check(pair,{0,255,0,255});
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../port/matrixgame/deko/mesh_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace srhd_awa::matrixgame {
// Controlled GPU readback, never a software rendering substitute. Same 13
// interior color controls as the real Windows D3D9 HAL/REF reference, plus a
// two-draw descriptor-isolation check. Requires a 64x64 renderer.
inline void ProbeSamplerGpu(DekoMeshRenderer& renderer) {
  TextureAsset asset;
  for(unsigned level=0;level<7;++level) {
    const auto side=64u>>level;
    TextureLevel image{side,side,side*4,std::vector<std::byte>(side*side*4)};
    for(unsigned i=0;i<side*side;++i){image.bytes[i*4]=std::byte(32+32*level);image.bytes[i*4+3]=std::byte{255};}
    asset.levels.push_back(std::move(image));
  }
  renderer.SetTextureAsset(1,asset);
  const std::array<std::byte,16> pattern{
    std::byte{16},std::byte{0},std::byte{0},std::byte{255},std::byte{80},std::byte{0},std::byte{0},std::byte{255},
    std::byte{144},std::byte{0},std::byte{0},std::byte{255},std::byte{208},std::byte{0},std::byte{0},std::byte{255}};
  renderer.SetTextureRGBA8(2,2,2,8,pattern);
  const std::array<std::uint16_t,6> indices{0,1,2,2,1,3};
  const auto quad=[](float extent,float u,float v) {
    return std::array{
      MeshVertex{{-1,1,.5f},{0,0,1},{u,v}},MeshVertex{{1,1,.5f},{0,0,1},{u+extent,v}},
      MeshVertex{{-1,-1,.5f},{0,0,1},{u,v+extent}},MeshVertex{{1,-1,.5f},{0,0,1},{u+extent,v+extent}}};
  };
  MeshSubmission submission{};submission.range={0,0,0,6,0,4};submission.draw.light[3]=1;
  for(unsigned i=0;i<4;++i)submission.draw.clip_from_model[i*5]=submission.draw.normal_from_model[i*5]=1;
  std::vector<std::byte> pixels(64*64*4);
  const auto check_pixels=[&](unsigned red,unsigned tolerance,unsigned x0=8,unsigned x1=56) {
    for(unsigned y=8;y<56;++y)for(unsigned x=x0;x<x1;++x) {
      const auto i=(y*64+x)*4;
      if(std::abs(int(std::to_integer<unsigned>(pixels[i]))-int(red))>int(tolerance)||
         pixels[i+1]!=std::byte{0}||pixels[i+2]!=std::byte{0}||pixels[i+3]!=std::byte{255})
        throw std::runtime_error("GPU sampler pixel mismatch");
    }
  };
  const auto check=[&](unsigned slot,const MeshSamplerState& state,float extent,float u,float v,unsigned red,unsigned tolerance=0) {
    renderer.SetMesh(quad(extent,u,v),indices);
    submission.texture_slot=slot;submission.sampling=state;
    renderer.DrawFrame(std::span(&submission,1),pixels);check_pixels(red,tolerance);
  };
  MeshSamplerState state;state.mip=TextureMipFilter::Linear;state.lod_bias=.25f;
  check(1,state,2,0,0,72,1);
  state.mip=TextureMipFilter::Point;check(1,state,2,0,0,64);
  state.mip=TextureMipFilter::None;check(1,state,2,0,0,32);
  state.mip=TextureMipFilter::Linear;state.lod_bias=-1;check(1,state,2,0,0,32);
  state.lod_bias=1;check(1,state,2,0,0,96);
  state=MeshSamplerState{};check(2,state,0,1.25f,-.25f,144);
  state.u=TextureAddress::Clamp;check(2,state,0,1.25f,-.25f,208);
  state.v=TextureAddress::Clamp;check(2,state,0,1.25f,-.25f,80);
  state.u=TextureAddress::Repeat;check(2,state,0,1.25f,-.25f,16);
  state.u=state.v=TextureAddress::Clamp;state.mag=TextureFilter::Linear;check(2,state,0,.5f,.5f,112,1);
  state.mag=TextureFilter::Point;state.min=TextureFilter::Linear;check(2,state,0,.5f,.5f,208);
  state=MeshSamplerState{};state.mag=TextureFilter::Linear;check(2,state,128,0,0,16);
  state.mag=TextureFilter::Point;state.min=TextureFilter::Linear;check(2,state,128,0,0,112,1);
  // Two samplers for the same texture within one command list. Reusing or
  // overwriting a single descriptor would turn both halves the same color.
  renderer.SetMesh(quad(0,1.25f,-.25f),indices);
  submission.sampling=MeshSamplerState{};submission.draw.clip_from_model[0]=.5f;submission.draw.clip_from_model[12]=-.5f;
  auto right=submission;right.draw.clip_from_model[12]=.5f;
  right.sampling.u=right.sampling.v=TextureAddress::Clamp;
  const std::array pair{submission,right};renderer.DrawFrame(pair,pixels);
  check_pixels(144,0,4,28);check_pixels(80,0,36,60);
}
}

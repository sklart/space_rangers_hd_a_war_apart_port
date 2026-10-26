// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_device_state.hpp"
#include "native_state_layout.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <type_traits>
namespace matrixgame_draw_audit {
namespace p=srhd_awa::matrixgame::platform3g;
inline constexpr std::array<unsigned,9> StageKeys{1,2,3,4,5,6,11,24,28};
inline constexpr std::array<unsigned,11> TransformKeys{256,2,3,16,17,18,19,20,21,22,23};
// Only value state, never a COM pointer or a fabricated native GPU resource.
struct State {
 std::uint32_t topology{},indexed{},fvf{},bound{},light_present{},depth_format{},multisample{},feedback{};
 std::array<std::uint32_t,103> render{};
 std::array<std::array<std::uint32_t,13>,8> sampler{};
 std::array<std::array<std::uint32_t,9>,8> stage{};
 std::array<std::array<float,16>,11> transform{};
 p::VideoViewport viewport{};p::VideoMaterial material{};
 std::array<p::VideoLight,8> lights{};std::array<std::uint32_t,8> light_enabled{};
};
static_assert(std::is_trivially_copyable_v<State> && std::endian::native==std::endian::little);
static_assert(sizeof(State)==2808); // Version 1 little-endian value-only capture ABI.
static_assert(sizeof(p::VideoLight)==104 && sizeof(p::VideoMaterial)==68 && sizeof(p::VideoViewport)==24);
inline p::NativeRenderState Build(const State& s){
 p::NativeRenderState n;
 for(unsigned i=0;i<p::NativeRenderWords.size();++i){const auto id=p::NativeRenderWords[i].id;
  // HAL's MSAA enable defaults to TRUE even with non-multisampled targets.
  // No MSAA semantics exist in that target; do not misclassify this raw SDK
  // default as an actual native request to allocate a multisample pipeline.
  if(id==161 && !s.multisample)n.Render(id,0);else n.Render(id,s.render[i]);
 }
 n.vertex.Layout(s.fvf);n.vertex.Viewport(s.viewport);n.vertex.Material(s.material);
 for(unsigned i=0;i<TransformKeys.size();++i)n.vertex.Transform(TransformKeys[i],s.transform[i]);
 for(unsigned slot=0;slot<8;++slot){
  for(unsigned i=0;i<13;++i)n.Sampler(slot,i+1,s.sampler[slot][i]);
  for(unsigned i=0;i<StageKeys.size();++i){const auto id=StageKeys[i];
   if(id==11 || id==24)n.vertex.Stage(slot,id,s.stage[slot][i]);else n.combiner.Set(slot,id,s.stage[slot][i]);}
  n.combiner.Bind(slot,(s.bound&(1u<<slot))!=0);
  if(s.light_present&(1u<<slot))n.vertex.Light(slot,s.lights[slot]);
  n.vertex.EnableLight(slot,s.light_enabled[slot]!=0);
 }
 if(s.feedback)throw std::runtime_error("actual render-target texture feedback");
 return n;
}
inline void Validate(const State& s){
 auto n=Build(s);n.ValidateDraw(s.topology==1,s.topology==2 || s.topology==3);
 srhd_awa::matrixgame::RequireStencilAttachment(n.Stencil(),s.depth_format!=0,s.depth_format);
 (void)n.vertex.Prepare();(void)n.combiner.Prepare();(void)n.PixelFog();
 for(const auto& sampler:n.samplers)sampler.Validate();
}
inline constexpr std::uint32_t Magic=0x3647524d;
}

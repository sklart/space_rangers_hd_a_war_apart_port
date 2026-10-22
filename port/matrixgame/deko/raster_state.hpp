// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame {
// These are serialized legacy register/enum IDs, never pointers or fake D3D APIs.
enum class RasterRegister : std::uint32_t {
  DepthTest=7,ShadeMode=9,DepthWrite=14,AlphaTest=15,SourceBlend=19,DestinationBlend=20,CullMode=22,
  DepthCompare=23,AlphaReference=24,AlphaCompare=25,BlendEnable=27,Lighting=137,ColorWrite=168,BlendOperation=171
};
enum class RasterCompare : std::uint32_t { Never=1,Less,Equal,LessEqual,Greater,NotEqual,GreaterEqual,Always };
enum class RasterBlendFactor : std::uint32_t {
  Zero=1,One,SourceColor,InverseSourceColor,SourceAlpha,InverseSourceAlpha,
  DestinationAlpha,InverseDestinationAlpha,DestinationColor,InverseDestinationColor,SourceAlphaSaturate
};
enum class RasterBlendOperation : std::uint32_t { Add=1,Subtract,ReverseSubtract,Minimum,Maximum };
struct MeshRasterState {
  bool depth_test{true},depth_write{true},alpha_test{},blend_enable{};
  RasterCompare depth_compare{RasterCompare::LessEqual},alpha_compare{RasterCompare::GreaterEqual};
  std::uint32_t alpha_reference{8},color_write{15};
  RasterBlendFactor source{RasterBlendFactor::SourceAlpha},destination{RasterBlendFactor::InverseSourceAlpha};
  RasterBlendOperation operation{RasterBlendOperation::Add};
  void Validate() const {
    const auto a=static_cast<unsigned>(alpha_compare),d=static_cast<unsigned>(depth_compare);
    const auto s=static_cast<unsigned>(source),t=static_cast<unsigned>(destination),o=static_cast<unsigned>(operation);
    if(a<1||a>8||d<1||d>8||s<1||s>11||t<1||t>11||o<1||o>5||alpha_reference>255||color_write>15)
      throw std::invalid_argument("unsupported raster state value");
  }
  void Apply(RasterRegister reg,std::uint32_t value) {
    auto next=*this;
    const auto boolean=[&]{if(value>1)throw std::invalid_argument("raster BOOL/W-buffer value");return value!=0;};
    switch(reg) {
      case RasterRegister::DepthTest:next.depth_test=boolean();break;
      case RasterRegister::DepthWrite:next.depth_write=boolean();break;
      case RasterRegister::AlphaTest:next.alpha_test=boolean();break;
      case RasterRegister::BlendEnable:next.blend_enable=boolean();break;
      case RasterRegister::SourceBlend:next.source=static_cast<RasterBlendFactor>(value);break;
      case RasterRegister::DestinationBlend:next.destination=static_cast<RasterBlendFactor>(value);break;
      case RasterRegister::DepthCompare:next.depth_compare=static_cast<RasterCompare>(value);break;
      case RasterRegister::AlphaCompare:next.alpha_compare=static_cast<RasterCompare>(value);break;
      case RasterRegister::AlphaReference:next.alpha_reference=value;break;
      case RasterRegister::ColorWrite:next.color_write=value;break;
      case RasterRegister::BlendOperation:next.operation=static_cast<RasterBlendOperation>(value);break;
      default:throw std::invalid_argument("unsupported raster register");
    }
    next.Validate();*this=next;
  }
  std::array<std::uint32_t,11> Words() const noexcept {
    return {depth_test,depth_write,alpha_test,static_cast<unsigned>(source),static_cast<unsigned>(destination),
      static_cast<unsigned>(depth_compare),alpha_reference,static_cast<unsigned>(alpha_compare),blend_enable,color_write,static_cast<unsigned>(operation)};
  }
};
inline constexpr std::array<RasterRegister,11> RasterRegisters{
  RasterRegister::DepthTest,RasterRegister::DepthWrite,RasterRegister::AlphaTest,RasterRegister::SourceBlend,
  RasterRegister::DestinationBlend,RasterRegister::DepthCompare,RasterRegister::AlphaReference,
  RasterRegister::AlphaCompare,RasterRegister::BlendEnable,RasterRegister::ColorWrite,RasterRegister::BlendOperation};
}

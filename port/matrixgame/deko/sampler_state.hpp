// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame {
// Original numeric state IDs, not a D3D compatibility API. Unsupported states
// fail explicitly; anisotropy, border/sRGB and displacement are not implemented.
enum class SamplerRegister : std::uint32_t {
  AddressU=1,AddressV=2,AddressW=3,MagFilter=5,MinFilter=6,MipFilter=7,LodBias=8
};
enum class TextureAddress : std::uint32_t { Repeat=1,Clamp=3 };
enum class TextureFilter : std::uint32_t { Point=1,Linear=2 };
enum class TextureMipFilter : std::uint32_t { None=0,Point=1,Linear=2 };
// Retained explicit presets for existing diagnostic callers.
enum class TextureSampling : std::uint32_t {
  NearestRepeat,LinearRepeat,NearestClamp,LinearClamp,
  NearestMipRepeat,LinearMipRepeat,NearestMipClamp,LinearMipClamp
};
struct MeshSamplerState {
  TextureAddress u{TextureAddress::Repeat},v{TextureAddress::Repeat},w{TextureAddress::Repeat};
  TextureFilter mag{TextureFilter::Point},min{TextureFilter::Point};
  TextureMipFilter mip{TextureMipFilter::None};
  float lod_bias{};
  MeshSamplerState()=default;
  MeshSamplerState(TextureSampling preset) {
    const auto bits=static_cast<std::uint32_t>(preset);
    if(bits>7)throw std::invalid_argument("unsupported sampler preset");
    min=mag=(bits&1)?TextureFilter::Linear:TextureFilter::Point;
    u=v=w=(bits&2)?TextureAddress::Clamp:TextureAddress::Repeat;
    mip=(bits&4)?((bits&1)?TextureMipFilter::Linear:TextureMipFilter::Point):TextureMipFilter::None;
  }
  void Validate() const {
    const auto address=[](TextureAddress a){return a==TextureAddress::Repeat||a==TextureAddress::Clamp;};
    const auto filter=[](TextureFilter f){return f==TextureFilter::Point||f==TextureFilter::Linear;};
    if(!address(u)||!address(v)||!address(w)||!filter(min)||!filter(mag)||
       static_cast<std::uint32_t>(mip)>2||!std::isfinite(lod_bias))
      throw std::invalid_argument("unsupported sampler state value");
  }
  void Apply(SamplerRegister reg,std::uint32_t value) {
    auto next=*this;
    switch(reg) {
      case SamplerRegister::AddressU:next.u=static_cast<TextureAddress>(value);break;
      case SamplerRegister::AddressV:next.v=static_cast<TextureAddress>(value);break;
      case SamplerRegister::AddressW:next.w=static_cast<TextureAddress>(value);break;
      case SamplerRegister::MagFilter:next.mag=static_cast<TextureFilter>(value);break;
      case SamplerRegister::MinFilter:next.min=static_cast<TextureFilter>(value);break;
      case SamplerRegister::MipFilter:next.mip=static_cast<TextureMipFilter>(value);break;
      case SamplerRegister::LodBias:next.lod_bias=std::bit_cast<float>(value);break;
      default:throw std::invalid_argument("unsupported sampler register");
    }
    next.Validate();*this=next;
  }
  std::array<std::uint32_t,7> Words() const noexcept {
    return {static_cast<unsigned>(u),static_cast<unsigned>(v),static_cast<unsigned>(w),
      static_cast<unsigned>(mag),static_cast<unsigned>(min),static_cast<unsigned>(mip),std::bit_cast<std::uint32_t>(lod_bias)};
  }
  bool operator==(const MeshSamplerState&) const=default;
};
inline constexpr std::array<SamplerRegister,7> SamplerRegisters{
  SamplerRegister::AddressU,SamplerRegister::AddressV,SamplerRegister::AddressW,
  SamplerRegister::MagFilter,SamplerRegister::MinFilter,SamplerRegister::MipFilter,SamplerRegister::LodBias};
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace srhd_awa::matrixgame::materials {
using Color=std::array<float,4>;
// Serialized D3D9 texture-stage registers, not SDK objects or fabricated caps.
struct alignas(16) Stage {
    std::array<std::uint32_t,4> color{1,2,1,1}; // op,arg1,arg2,result
    std::array<std::uint32_t,4> alpha{1,2,1,0}; // op,arg1,arg2,prepared flags: sample/null/opaque initial alpha
    bool operator==(const Stage&)const=default;
};
struct alignas(16) Uniforms {
    std::array<Stage,8> stages{};
    Color factor{1,1,1,1};
};
static_assert(sizeof(Stage)==32 && sizeof(Uniforms)==272);
static_assert(offsetof(Uniforms,factor)==256);
struct Incomplete:std::runtime_error {using std::runtime_error::runtime_error;};
class Combiner {
public:
    Combiner();
    void Set(std::uint32_t stage,std::uint32_t reg,std::uint32_t value);
    std::uint32_t Get(std::uint32_t stage,std::uint32_t reg)const;
    void Factor(std::uint32_t argb)noexcept;
    void Bind(std::uint32_t stage,bool present);
    // This validates a draw, not individual setters: the original callbacks
    // change op/arguments/result in separate calls and leave transient states.
    Uniforms Prepare()const;
    const Uniforms& State()const noexcept{return state_;}
    const std::array<bool,8>& Bound()const noexcept{return bound_;}
private:
    Uniforms state_;
    std::array<bool,8> bound_{};
};
// Scalar diagnostic oracle only. Rendering is performed by material.frag.glsl.
// Original raw stage registers/null bindings remain unchanged. Prepare builds
// effective cascade/routing; reference tests cover original alpha/null cases.
// Physical R8 stores logical A8; alpha output remains intact for alpha test/blend.
inline void AlphaTarget(Uniforms& u,bool enabled)noexcept{if(enabled)u.stages[0].alpha[3]|=8u;else u.stages[0].alpha[3]&=~8u;}
Color Evaluate(const Uniforms&,Color diffuse,const std::array<Color,8>& samples);
}

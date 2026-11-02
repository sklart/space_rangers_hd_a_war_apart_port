// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../native-engine/native_gamma.hpp"
#include <deko3d.hpp>
#include <span>
namespace srhd_awa::matrixgame {
class DekoGammaProgram {
    dk::Device device_;
    dk::UniqueMemBlock code_[2];dk::Shader shaders_[2];
public:
    // Immutable per-submission LUT and descriptors; retain through wait/close.
    struct Packet {dk::UniqueMemBlock memory;};
    DekoGammaProgram(dk::Device,std::span<const std::byte> vertex,std::span<const std::byte> fragment);
    Packet Prepare(const dk::Image& source,const platform3g::GammaRamp&)const;
    void Record(dk::CmdBuf,const Packet&,const dk::Image& output,unsigned width,unsigned height)const;
};
}

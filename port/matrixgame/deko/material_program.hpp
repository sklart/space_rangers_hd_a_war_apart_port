// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "material_combiner.hpp"
#include "fixed_vertex.hpp"
#include "pixel_fog.hpp"
#include <deko3d.hpp>
#include <memory>
#include <span>

namespace srhd_awa::matrixgame::materials {
// Texture cascade and optional original fixed vertex lighting/TCI program.
// Raster state, texture owners and frame lifecycle belong to the renderer.
// Shader validity does not by itself prove full native Init or a GPU frame.
class DekoCombinerProgram {
public:
    struct Packet {
        dk::UniqueMemBlock uniforms;
        dk::UniqueMemBlock vertex_uniforms;
        std::array<DkResHandle,8> textures;
    };
    DekoCombinerProgram(dk::Device,std::span<const std::byte> fragment_dksh,std::span<const std::byte> vertex_dksh={});
    bool VertexReady()const noexcept;
    Packet Prepare(const Combiner&,const std::array<DkResHandle,8>&,const VertexUniforms* vertex=nullptr,bool alpha_target=false,const PixelFogUniforms* fog=nullptr,std::uint32_t shade=2)const;
    // Packet, descriptor sets, textures and this program must outlive queue
    // completion; this does not steal another renderer's queue or submit it.
    void Bind(dk::CmdBuf,const Packet&)const;
private:
    dk::Device device_;
    dk::UniqueMemBlock code_;
    dk::UniqueMemBlock vertex_code_;
    dk::Shader shader_;
    dk::Shader vertex_shader_;
};
}

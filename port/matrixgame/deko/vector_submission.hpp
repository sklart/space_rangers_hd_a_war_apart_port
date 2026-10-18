// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../native-core/vector_geometry.hpp"
namespace srhd_awa::matrixgame {
struct VectorMaterial { MeshDraw draw; std::uint32_t texture_slot; MeshSamplerState sampling; MeshRasterState raster{}; };
// Explicit single-pass skin input. Missing materials/textures do not silently
// become white. Additional original skin passes must be submitted by the engine.
inline std::vector<MeshSubmission> PlanVectorFrame(const VectorGeometry& model,std::uint32_t frame,
    std::span<const VectorMaterial> materials) {
  if(materials.size()!=model.Surfaces().size()) throw std::invalid_argument("VO material count");
  auto ranges=model.FrameRanges(frame);std::vector<MeshSubmission> out;out.reserve(ranges.size());
  for(const auto& range:ranges) {
    const auto& material=materials[range.surface];
    material.raster.Validate();
    material.sampling.Validate();
    out.push_back({range,material.draw,material.texture_slot,material.sampling,material.raster});
  }
  return out;
}
inline void UploadVectorGeometry(DekoMeshRenderer& renderer,const VectorGeometry& model) {
  renderer.SetMesh(model.Vertices(),model.Indices());
}
}

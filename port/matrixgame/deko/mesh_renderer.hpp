// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "mesh_submission.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace srhd_awa::matrixgame {
// MatrixLib/3G/VectorObject.hpp SVOVertex (XYZ | NORMAL | TEX1).
// This boundary keeps every original vertex component; it performs no remeshing.
struct MeshVertex { float position[3], normal[3], uv[2]; };
static_assert(sizeof(MeshVertex)==32 && offsetof(MeshVertex,normal)==12 && offsetof(MeshVertex,uv)==24);
struct MeshDraw {
  // Column-major GPU matrices. The caller owns D3DX row-vector conversion;
  // the renderer does not change geometry, camera or game simulation.
  std::array<float,16> clip_from_model{}, normal_from_model{};
  std::array<float,4> diffuse{1,1,1,1};
  // xyz = unit direction toward light, w = ambient contribution (0..1).
  std::array<float,4> light{0,0,1,.2f};
};
struct MeshSubmission { MeshRange range; MeshDraw draw; };
// First offscreen GPU geometry pass only. No swapchain, texture/material parity,
// battle entry, software fallback, or campaign result is implemented by this class.
// One owning render thread; all work completes before readback or resource reuse.
class DekoMeshRenderer {
 public:
  DekoMeshRenderer(std::uint32_t width, std::uint32_t height,
      std::span<const std::byte> vertex_dksh, std::span<const std::byte> fragment_dksh);
  ~DekoMeshRenderer();
  DekoMeshRenderer(const DekoMeshRenderer&)=delete;
  DekoMeshRenderer& operator=(const DekoMeshRenderer&)=delete;
  void SetMesh(std::span<const MeshVertex>, std::span<const std::uint16_t> index_buffer);
  // Writes tightly packed RGBA8 rows after GPU queue completion. This is GPU
  // readback for a later physical probe, never an emulated host framebuffer.
  void Draw(const MeshDraw&, std::span<std::byte> rgba_output);
  // Preserve native submesh order, first index and signed base vertex. One
  // clear/readback per batch; each draw owns a separate uniform slot.
  void DrawFrame(std::span<const MeshSubmission>, std::span<std::byte> rgba_output);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace srhd_awa::matrixgame

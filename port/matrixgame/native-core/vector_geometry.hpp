// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VectorLayout.hpp"
#include "../deko/mesh_renderer.hpp"
#include <optional>
#include <string>
#include <vector>

namespace Base { class CStorage; }
namespace srhd_awa::matrixgame {
// Owned CPU data of the actual STRG VectorObject format. No D3D resources or
// invented materials are created by loading it. Unused columns remain in STRG.
struct VectorAnimation { std::u16string name; std::uint32_t id{}, first{}, count{}; };
struct VectorMatrix { std::u16string name; std::uint32_t id{}, first{}; };
struct VectorSimpleGeometry {
  vo::SVOGeometrySimple::SVOGS_asis bounds;
  std::vector<vo::SVOTriangle> triangles;
  std::vector<D3DXVECTOR3> vertices;
};
class VectorGeometry {
 public:
  static VectorGeometry Load(Base::CStorage&);
  static VectorGeometry Load(const wchar_t* path);
  std::span<const MeshVertex> Vertices() const noexcept { return vertices_; }
  std::span<const std::uint16_t> Indices() const noexcept { return indices_; }
  std::span<const vo::SVOKadr> Frames() const noexcept { return frames_; }
  std::span<const vo::SVOUnion> Groups() const noexcept { return groups_; }
  std::span<const vo::SVOKadrEdge> Edges() const noexcept { return edges_; }
  std::span<const std::uint32_t> FrameVertices() const noexcept { return frame_vertices_; }
  std::span<const std::uint32_t> FrameGroups() const noexcept { return frame_groups_; }
  std::span<const vo::SVOFrameIndex> AnimationFrames() const noexcept { return animation_frames_; }
  std::span<const VectorAnimation> Animations() const noexcept { return animations_; }
  std::span<const VectorMatrix> MatrixNames() const noexcept { return matrix_names_; }
  std::span<const D3DXMATRIX> Matrices() const noexcept { return matrices_; }
  const auto& Surfaces() const noexcept { return surfaces_; }
  const auto& Properties() const noexcept { return properties_; }
  const auto& Simple() const noexcept { return simple_; }
  // One material pass only. Original CVectorObject::Draw visits surfaces first,
  // then frame union IDs. The caller supplies actual skin passes/material state.
  std::vector<MeshRange> FrameRanges(std::uint32_t frame) const;
 private:
  std::vector<MeshVertex> vertices_;
  std::vector<std::uint16_t> indices_;
  std::vector<vo::SVOKadr> frames_;
  std::vector<vo::SVOUnion> groups_;
  std::vector<vo::SVOKadrEdge> edges_;
  std::vector<std::uint32_t> frame_vertices_, frame_groups_;
  std::vector<vo::SVOFrameIndex> animation_frames_;
  std::vector<VectorAnimation> animations_;
  std::vector<VectorMatrix> matrix_names_;
  std::vector<D3DXMATRIX> matrices_;
  std::vector<std::u16string> surfaces_;
  std::vector<std::pair<std::u16string,std::u16string>> properties_;
  std::optional<VectorSimpleGeometry> simple_;
};
}

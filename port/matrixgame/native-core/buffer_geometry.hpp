// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_resources.hpp"
#include "../deko/mesh_renderer.hpp"

namespace srhd_awa::matrixgame::buffers {
inline constexpr std::uint32_t VoVertexLayout=0x112; // XYZ | NORMAL | TEX1
struct UploadKey {
    std::uint64_t vertex_id{},vertex_version{},index_id{},index_version{};
    std::size_t vertex_offset{},vertex_stride{};
    bool operator==(const UploadKey&) const=default;
};
struct Geometry {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint16_t> indices;
    UploadKey key;
};
// Only the implemented VO vertex layout and u16 index path are accepted. Terrain
// layouts/u32 GPU draws are explicit remaining blockers, never converted away.
UploadKey ValidateBoundVO(const Context&,std::span<const MeshRange>);
Geometry CaptureBoundVO(const Context&,std::span<const MeshRange>);
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../native-core/buffer_geometry.hpp"
#include <optional>
namespace srhd_awa::matrixgame {
// Real staging -> GPU SetMesh -> indexed draw path. Uploads are retained until
// source bytes/bindings change or somebody replaces the renderer's mesh.
class DekoBufferRenderer {
public:
    explicit DekoBufferRenderer(DekoMeshRenderer& renderer):renderer_(renderer) {}
    void Draw(buffers::Context&,std::span<const MeshSubmission>,std::span<std::byte> rgba_output);
private:
    DekoMeshRenderer& renderer_;
    std::optional<buffers::UploadKey> uploaded_;
    std::uint64_t mesh_revision_{};
};
}

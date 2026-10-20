// SPDX-License-Identifier: GPL-2.0-or-later
#include "buffer_renderer.hpp"
namespace srhd_awa::matrixgame {
void DekoBufferRenderer::Draw(buffers::Context& context,std::span<const MeshSubmission> submissions,
                             std::span<std::byte> output) {
    if(submissions.empty()) {renderer_.DrawFrame(submissions,output);return;}
    std::vector<MeshRange> ranges;ranges.reserve(submissions.size());
    for(const auto& submission:submissions) ranges.push_back(submission.range);
    const auto key=buffers::ValidateBoundVO(context,ranges);
    if(!uploaded_ || *uploaded_!=key || mesh_revision_!=renderer_.MeshRevision()) {
        const auto geometry=buffers::CaptureBoundVO(context,ranges);
        renderer_.SetMesh(geometry.vertices,geometry.indices);
        uploaded_=key;mesh_revision_=renderer_.MeshRevision();
    }
    renderer_.DrawFrame(submissions,output);
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ShadowStencil.hpp"
#include <span>
namespace srhd_awa::matrixgame {
// Read-only borrowed diagnostics. Build/free/destruction invalidates these views.
struct ShadowStencilView {
    std::span<const SVOShadowStencilVertex> vertices;
    std::span<const std::uint16_t> indices;
    const buffers::VertexBuffer* vertex_buffer{};
    const buffers::IndexBuffer* index_buffer{};
    int frame{-1};
};
struct ShadowStencilViewAccess {
    static ShadowStencilView Read(const CVOShadowStencil& object) {
        ShadowStencilView result;result.vertex_buffer=object.m_VB;result.index_buffer=object.m_IB;
        result.frame=object.m_FrameFor;
        if(result.frame>=0) {
            const auto& frame=object.m_Frames.at(result.frame);
            result.vertices=frame.m_preVerts;result.indices=frame.m_preInds;
        }
        return result;
    }
    static std::size_t LiveObjects() noexcept {
        std::size_t count{};for(auto* item=CVOShadowStencil::m_First;item;item=item->m_Next)++count;
        return count;
    }
};
}

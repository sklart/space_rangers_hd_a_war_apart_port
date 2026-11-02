// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_resources.hpp"
#include "platform_3g.hpp"
namespace srhd_awa::matrixgame::native_staging {
// Setters for the authoritative CPU staging consumed by NativeDraw/Indexed.
// GPU bytes are uploaded by the real draw path, not by a comparison-only bind.
inline platform3g::DeviceResult BindVertex(std::uint32_t stream,buffers::VertexBuffer* buffer,std::uint32_t offset,std::uint32_t stride){
    if(stream)throw platform3g::Unsupported("native staging supports stream zero only");
    if(buffer && (buffer->Locked() || !stride || offset>=buffer->Size()))throw std::invalid_argument("invalid native staging vertex binding");
    auto& context=buffers::Current();
    context.BindVertex(buffer,offset,stride); // Validates owner, retains new before releasing old.
    const auto bound=context.Vertex();
    if(bound.buffer!=buffer || bound.offset!=(buffer?offset:0) || bound.stride!=(buffer?stride:0))
        throw std::logic_error("native staging vertex setter postcondition");
    return platform3g::DeviceSuccess;
}
inline platform3g::DeviceResult BindIndex(buffers::IndexBuffer* buffer){
    if(buffer && buffer->Locked())throw std::invalid_argument("locked native staging index binding");
    auto& context=buffers::Current();context.BindIndex(buffer);
    if(context.Index()!=buffer)throw std::logic_error("native staging index setter postcondition");
    return platform3g::DeviceSuccess;
}
}

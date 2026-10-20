// SPDX-License-Identifier: GPL-2.0-or-later
#include "buffer_geometry.hpp"
#include <cstring>
#include <stdexcept>
namespace srhd_awa::matrixgame::buffers {
UploadKey ValidateBoundVO(const Context& context,std::span<const MeshRange> ranges) {
    const auto vertex=context.Vertex();const auto* index=context.Index();
    if(!vertex.buffer || !index) throw std::logic_error("geometry buffers are not bound");
    if(vertex.buffer->Locked() || index->Locked()) throw std::logic_error("geometry buffer is mapped");
    if(vertex.buffer->Layout()!=VoVertexLayout || vertex.stride!=sizeof(MeshVertex) || index->Width()!=IndexWidth::U16)
        throw std::invalid_argument("GPU buffer layout is not implemented");
    const auto bytes=vertex.buffer->Size()-vertex.offset;
    if(bytes%vertex.stride || index->Size()%sizeof(std::uint16_t) ||
       bytes/vertex.stride>MaxMeshVertices || index->Size()/sizeof(std::uint16_t)>MaxMeshIndices)
        throw std::out_of_range("bound mesh extent");
    const auto available=bytes/vertex.stride;const auto indices=index->Storage();
    for(const auto& range:ranges) {
        if(range.index_count%3 || std::uint64_t(range.first_index)+range.index_count>indices.size()/2)
            throw std::out_of_range("bound draw indices");
        if(!index->Initialized(std::size_t(range.first_index)*2,std::size_t(range.index_count)*2))
            throw std::logic_error("draw reads unwritten indices");
        const auto end=std::uint64_t(range.minimum_vertex)+range.vertex_count;
        for(std::size_t i=range.first_index;i<std::uint64_t(range.first_index)+range.index_count;++i) {
            std::uint16_t value;std::memcpy(&value,indices.data()+i*2,2);
            const auto effective=std::int64_t(range.base_vertex)+value;
            if(value<range.minimum_vertex || value>=end || effective<0 || std::uint64_t(effective)>=available)
                throw std::out_of_range("bound draw effective vertex");
            if(!vertex.buffer->Initialized(vertex.offset+std::size_t(effective)*vertex.stride,sizeof(MeshVertex)))
                throw std::logic_error("draw reads unwritten vertices");
        }
    }
    return {vertex.buffer->Id(),vertex.buffer->Version(),index->Id(),index->Version(),vertex.offset,vertex.stride};
}
Geometry CaptureBoundVO(const Context& context,std::span<const MeshRange> ranges) {
    Geometry result;result.key=ValidateBoundVO(context,ranges);
    const auto vertex=context.Vertex();const auto* index=context.Index();
    const auto vertices=vertex.buffer->Storage().subspan(vertex.offset);const auto indices=index->Storage();
    result.vertices.resize(vertices.size()/sizeof(MeshVertex));result.indices.resize(indices.size()/2);
    std::memcpy(result.vertices.data(),vertices.data(),vertices.size());
    std::memcpy(result.indices.data(),indices.data(),indices.size());
    return result;
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "BigVB.hpp"
#include "BigIB.hpp"
#include "buffer_geometry.hpp"
#include "vector_geometry.hpp"
#include <memory>
#include <type_traits>
namespace srhd_awa::matrixgame {
// Owns VO payload copies and original packing containers. This is a geometry
// resource owner, not CVectorObject/skin/animation or an engine entry point.
class PackedVectorBuffers {
    struct Vertex : MeshVertex { static constexpr std::uint32_t FVF=buffers::VoVertexLayout; };
    static_assert(sizeof(Vertex)==32&&std::is_standard_layout_v<Vertex>&&std::is_trivially_copyable_v<Vertex>);
    struct DeleteVertices {void operator()(CBigVB<Vertex>* p) const noexcept {CBigVB<Vertex>::DestroyBigVB(p);}};
    struct DeleteIndices {void operator()(CBigIB* p) const noexcept {CBigIB::DestroyBigIB(p);}};
    std::vector<Vertex> vertices_;
    std::vector<std::uint16_t> indices_;
    SBigVBSource<Vertex> vertex_source_;
    SBigIBSource index_source_;
    std::unique_ptr<CBigVB<Vertex>,DeleteVertices> vertex_owner_;
    std::unique_ptr<CBigIB,DeleteIndices> index_owner_;
public:
    explicit PackedVectorBuffers(const VectorGeometry& model) {
        if(model.Vertices().empty()||model.Indices().empty()||model.Vertices().size()>BVB_MAX_SIZE/sizeof(Vertex)||model.Indices().size()>BIB_MAX_SIZE/sizeof(std::uint16_t))
            throw std::length_error("VO exceeds original per-source packing extent");
        vertices_.resize(model.Vertices().size());indices_.assign(model.Indices().begin(),model.Indices().end());
        std::memcpy(vertices_.data(),model.Vertices().data(),model.Vertices().size_bytes());
        vertex_source_.verts=vertices_.data();vertex_source_.size=static_cast<int>(vertices_.size()*sizeof(Vertex));
        index_source_.inds=indices_.data();index_source_.size=static_cast<int>(indices_.size()*2);
        vertex_owner_.reset(CBigVB<Vertex>::NewBigVB(nullptr));index_owner_.reset(CBigIB::NewBigIB(nullptr));
        vertex_owner_->AddSource(&vertex_source_);index_owner_->AddSource(&index_source_);
    }
    PackedVectorBuffers(const PackedVectorBuffers&)=delete;
    PackedVectorBuffers& operator=(const PackedVectorBuffers&)=delete;
    PackedVectorBuffers(PackedVectorBuffers&&)=delete; // packing parts borrow source addresses
    PackedVectorBuffers& operator=(PackedVectorBuffers&&)=delete;
    std::pair<std::int32_t,std::uint32_t> Select(buffers::Context& context) {
        buffers::Scope scope(context);
        vertex_owner_->BeforeDraw();index_owner_->BeforeDraw();
        const auto vertex=vertex_source_.Select(vertex_owner_.get());const auto index=index_source_.Select(index_owner_.get());
        if(vertex<0||index<0) throw std::bad_alloc(); // CPU staging failure; GPU upload still follows
        return {vertex,static_cast<std::uint32_t>(index)};
    }
    void ReleaseBuffers() {vertex_owner_->ReleaseBuffers();index_owner_->ReleaseBuffers();}
};
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace srhd_awa::matrixgame::buffers {
enum class Pool { Default, Managed };
enum class IndexWidth : std::uint32_t { U16=2, U32=4 };
enum class MapMode { Normal, Discard, NoOverwrite };
struct AllocationState;

// Owned CPU staging, not a COM object or a declaration of GPU readiness. A
// successful map/unmap publishes bytes for a later real backend upload.
class Resource {
public:
    Resource(const Resource&)=delete;
    Resource& operator=(const Resource&)=delete;
    void Release() noexcept;
    void* Map(std::size_t offset,std::size_t bytes,MapMode mode=MapMode::Normal);
    void Unmap();
    std::size_t Size() const noexcept { return bytes_.size(); }
    std::uint64_t Id() const noexcept { return id_; }
    std::uint64_t Version() const noexcept { return version_; }
    bool Locked() const noexcept { return locked_; }
    bool Initialized(std::size_t offset,std::size_t bytes) const noexcept;
    std::span<const std::byte> Storage() const;
    Pool MemoryPool() const noexcept { return pool_; }
    bool Dynamic() const noexcept { return dynamic_; }
protected:
    Resource(std::shared_ptr<AllocationState>,std::size_t,Pool,bool);
    virtual ~Resource();
private:
    friend class Context;
    void Retain();
    struct Range {std::size_t first,last;};
    std::shared_ptr<AllocationState> allocation_;
    std::vector<std::byte> bytes_;
    std::vector<Range> initialized_;
    std::uint64_t id_{},version_{};
    std::uint32_t refs_{1};
    Pool pool_;
    bool dynamic_{},locked_{};
    Range mapping_{};
};
class VertexBuffer final : public Resource {
public:
    std::uint32_t Layout() const noexcept { return layout_; }
private:
    friend class Context;
    VertexBuffer(std::shared_ptr<AllocationState>,std::size_t,std::uint32_t,Pool,bool);
    std::uint32_t layout_;
};
class IndexBuffer final : public Resource {
public:
    IndexWidth Width() const noexcept { return width_; }
private:
    friend class Context;
    IndexBuffer(std::shared_ptr<AllocationState>,std::size_t,IndexWidth,Pool,bool);
    IndexWidth width_;
};
struct VertexBinding { const VertexBuffer* buffer{};std::size_t offset{},stride{}; };

// One owning engine/render thread. Bindings retain their storage after the
// source owner releases it; unbind/destruction drops those internal references.
class Context {
public:
    explicit Context(std::size_t byte_budget=std::numeric_limits<std::size_t>::max());
    ~Context();
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
    // Null denotes allocation/budget failure. Invalid descriptors throw.
    VertexBuffer* CreateVertex(std::int64_t bytes,std::uint32_t layout,Pool=Pool::Default,bool dynamic=false);
    IndexBuffer* CreateIndex(std::int64_t bytes,IndexWidth,Pool=Pool::Default,bool dynamic=false);
    void BindVertex(VertexBuffer*,std::size_t offset,std::size_t stride);
    void BindIndex(IndexBuffer*);
    void ClearBindings() noexcept;
    VertexBinding Vertex() const noexcept { return {vertex_,offset_,stride_}; }
    const IndexBuffer* Index() const noexcept { return index_; }
    std::size_t AllocatedBytes() const noexcept;
private:
    std::shared_ptr<AllocationState> allocation_;
    VertexBuffer* vertex_{};
    IndexBuffer* index_{};
    std::size_t offset_{},stride_{};
};
Context& Current(); // Missing engine binding is an error, never a fake device.
class Scope {
public:
    explicit Scope(Context&);
    ~Scope();
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
private:
    Context* previous_;
};
std::int64_t IndexBytes(std::int64_t count,IndexWidth);
template<class T> void MapPart(Resource* resource,std::size_t offset,std::size_t bytes,T** out,MapMode mode=MapMode::Normal) {
    if(!resource || !out) throw std::invalid_argument("buffer mapping target");
    *out=static_cast<T*>(resource->Map(offset,bytes,mode));
}
}

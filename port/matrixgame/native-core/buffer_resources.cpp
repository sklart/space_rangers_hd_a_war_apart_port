// SPDX-License-Identifier: GPL-2.0-or-later
#include "buffer_resources.hpp"
#include <algorithm>
#include <atomic>
#include <new>
#include <stdexcept>

namespace srhd_awa::matrixgame::buffers {
struct AllocationState { std::size_t budget{},bytes{}; };
namespace {
std::atomic<std::uint64_t> next_id{1};
thread_local Context* active=nullptr;
std::uint64_t Identity() {
    auto value=next_id.load(std::memory_order_relaxed);
    for(;;) {
        if(!value) throw std::overflow_error("buffer identity exhausted");
        const auto next=value==std::numeric_limits<std::uint64_t>::max()?0:value+1;
        if(next_id.compare_exchange_weak(value,next,std::memory_order_relaxed)) return value;
    }
}
std::size_t CheckedSize(std::int64_t bytes) {
    if(bytes<=0 || std::uint64_t(bytes)>std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("legacy buffer byte extent");
    return static_cast<std::size_t>(bytes);
}
}
Resource::Resource(std::shared_ptr<AllocationState> owner,std::size_t bytes,Pool pool,bool dynamic)
    :allocation_(std::move(owner)),bytes_(bytes),pool_(pool),dynamic_(dynamic) {
    id_=Identity();
    allocation_->bytes+=bytes;
}
Resource::~Resource() { allocation_->bytes-=bytes_.size(); }
void Resource::Retain() {
    if(refs_==std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("buffer references");
    ++refs_;
}
void Resource::Release() noexcept { if(--refs_==0) delete this; }
void* Resource::Map(std::size_t offset,std::size_t bytes,MapMode mode) {
    if(locked_) throw std::logic_error("buffer already mapped");
    if(mode!=MapMode::Normal && mode!=MapMode::Discard && mode!=MapMode::NoOverwrite)
        throw std::invalid_argument("buffer map mode");
    if(mode!=MapMode::Normal && !dynamic_) throw std::invalid_argument("dynamic buffer map required");
    if(!bytes && !offset) bytes=bytes_.size();
    if(!bytes || offset>bytes_.size() || bytes>bytes_.size()-offset)
        throw std::out_of_range("buffer map range");
    if(version_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("buffer write version");
    // Allocate range bookkeeping before exposing writable storage. Unmap does
    // not allocate and cannot strand an upload half-published on bad_alloc.
    initialized_.reserve(initialized_.size()+1);
    if(mode==MapMode::Discard) {
        initialized_.clear();std::fill(bytes_.begin(),bytes_.end(),std::byte{});
    }
    mapping_={offset,offset+bytes};locked_=true;
    return bytes_.data()+offset;
}
void Resource::Unmap() {
    if(!locked_) throw std::logic_error("buffer not mapped");
    initialized_.push_back(mapping_);
    std::sort(initialized_.begin(),initialized_.end(),[](Range a,Range b){return a.first<b.first;});
    std::size_t count=0;
    for(auto range:initialized_) {
        if(count && initialized_[count-1].last>=range.first)
            initialized_[count-1].last=std::max(initialized_[count-1].last,range.last);
        else initialized_[count++]=range;
    }
    initialized_.resize(count);locked_=false;++version_;
}
bool Resource::Initialized(std::size_t offset,std::size_t bytes) const noexcept {
    if(locked_ || offset>bytes_.size() || bytes>bytes_.size()-offset) return false;
    if(!bytes) return true;
    for(auto range:initialized_) {
        if(range.first>offset) return false;
        if(range.first<=offset && range.last>=offset+bytes) return true;
    }
    return false;
}
std::span<const std::byte> Resource::Storage() const {
    if(locked_) throw std::logic_error("mapped buffer cannot be consumed");
    return bytes_;
}
VertexBuffer::VertexBuffer(std::shared_ptr<AllocationState> owner,std::size_t bytes,std::uint32_t layout,Pool pool,bool dynamic)
    :Resource(std::move(owner),bytes,pool,dynamic),layout_(layout) {}
IndexBuffer::IndexBuffer(std::shared_ptr<AllocationState> owner,std::size_t bytes,IndexWidth width,Pool pool,bool dynamic)
    :Resource(std::move(owner),bytes,pool,dynamic),width_(width) {}
Context::Context(std::size_t budget):allocation_(std::make_shared<AllocationState>(AllocationState{budget,0})) {}
Context::~Context() { ClearBindings(); }
VertexBuffer* Context::CreateVertex(std::int64_t bytes,std::uint32_t layout,Pool pool,bool dynamic) {
    const auto size=CheckedSize(bytes);
    if(!layout || (pool!=Pool::Default&&pool!=Pool::Managed) || (dynamic&&pool==Pool::Managed))
        throw std::invalid_argument("vertex buffer descriptor");
    if(size>allocation_->budget-allocation_->bytes) return nullptr;
    try {return new VertexBuffer(allocation_,size,layout,pool,dynamic);}catch(const std::bad_alloc&){return nullptr;}
}
IndexBuffer* Context::CreateIndex(std::int64_t bytes,IndexWidth width,Pool pool,bool dynamic) {
    const auto size=CheckedSize(bytes);
    if((width!=IndexWidth::U16&&width!=IndexWidth::U32) || size%static_cast<std::size_t>(width) ||
       (pool!=Pool::Default&&pool!=Pool::Managed) || (dynamic&&pool==Pool::Managed))
        throw std::invalid_argument("index buffer descriptor");
    if(size>allocation_->budget-allocation_->bytes) return nullptr;
    try {return new IndexBuffer(allocation_,size,width,pool,dynamic);}catch(const std::bad_alloc&){return nullptr;}
}
void Context::BindVertex(VertexBuffer* buffer,std::size_t offset,std::size_t stride) {
    if(buffer && (buffer->allocation_!=allocation_ || !stride || offset>=buffer->Size()))
        throw std::invalid_argument("vertex buffer binding");
    if(buffer) buffer->Retain();
    if(vertex_) vertex_->Release();
    vertex_=buffer;offset_=buffer?offset:0;stride_=buffer?stride:0;
}
void Context::BindIndex(IndexBuffer* buffer) {
    if(buffer && buffer->allocation_!=allocation_) throw std::invalid_argument("index buffer owner");
    if(buffer) buffer->Retain();
    if(index_) index_->Release();
    index_=buffer;
}
void Context::ClearBindings() noexcept {
    if(vertex_) vertex_->Release();if(index_) index_->Release();
    vertex_=nullptr;index_=nullptr;offset_=stride_=0;
}
std::size_t Context::AllocatedBytes() const noexcept {return allocation_->bytes;}
Context& Current() {if(!active) throw std::logic_error("MatrixGame buffer context is not bound");return *active;}
Scope::Scope(Context& context):previous_(active) {
    if(active && active!=&context) throw std::logic_error("another MatrixGame buffer context is active");
    active=&context;
}
Scope::~Scope(){active=previous_;}
std::int64_t IndexBytes(std::int64_t count,IndexWidth width) {
    const auto size=static_cast<std::int64_t>(width);
    if((width!=IndexWidth::U16&&width!=IndexWidth::U32) || count<=0 || count>std::numeric_limits<std::uint32_t>::max()/size)
        throw std::invalid_argument("legacy index buffer count");
    return count*size;
}
}

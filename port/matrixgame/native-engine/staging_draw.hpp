// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_resources.hpp"
#include <array>
#include <cstring>
namespace srhd_awa::matrixgame::staging_draw {
inline std::uint64_t ElementCount(std::uint32_t kind,std::uint32_t primitives){
    if(!primitives)return 0;
    switch(kind){
    case 1:return primitives;
    case 2:return std::uint64_t(primitives)*2;
    case 3:return std::uint64_t(primitives)+1;
    case 4:return std::uint64_t(primitives)*3;
    case 5:case 6:return std::uint64_t(primitives)+2;
    default:throw std::invalid_argument("unknown staging primitive topology");
    }
}
inline void Vertex(const buffers::VertexBinding& binding,std::int64_t index){
    if(!binding.buffer || binding.buffer->Locked() || !binding.stride || index<0)throw std::out_of_range("staging vertex missing/negative");
    if(binding.offset>binding.buffer->Size() || std::uint64_t(index)>(binding.buffer->Size()-binding.offset)/binding.stride)
        throw std::out_of_range("staging vertex byte range");
    const auto offset=std::uint64_t(binding.offset)+std::uint64_t(index)*binding.stride;
    if(offset>binding.buffer->Size() || !binding.buffer->Initialized(static_cast<std::size_t>(offset),binding.stride))
        throw std::out_of_range("staging draw references uninitialized vertex");
}
// Consecutive vertices occupy one contiguous initialized range. Keep the
// original per-vertex validation for invalid or partially initialized input.
inline void VertexRange(const buffers::VertexBinding& binding,std::uint64_t start,std::uint64_t count){
    if(!count)return;
    if(binding.buffer && !binding.buffer->Locked() && binding.stride && binding.offset<=binding.buffer->Size()){
        const auto available=(binding.buffer->Size()-binding.offset)/binding.stride;
        if(start<=available && count<=available-start){
            const auto offset=std::uint64_t(binding.offset)+start*binding.stride;
            if(binding.buffer->Initialized(static_cast<std::size_t>(offset),static_cast<std::size_t>(count*binding.stride)))return;
        }
    }
    for(std::uint64_t i=0;i<count;++i)Vertex(binding,static_cast<std::int64_t>(start+i));
}
inline void ValidateIndexed(const buffers::Context& context,std::uint32_t topology,int base,
    std::uint32_t minimum,std::uint32_t vertices,std::uint32_t start,std::uint32_t primitives){
    const auto binding=context.Vertex();const auto* index=context.Index();
    if(!index || index->Locked())throw std::invalid_argument("staging index missing/locked");
    const auto width=static_cast<std::uint32_t>(index->Width());
    const auto first=std::uint64_t(start)*width,count=ElementCount(topology,primitives);
    if(first>index->Size() || count>(index->Size()-first)/width || !index->Initialized(first,count*width))
        throw std::out_of_range("staging index range");
    const auto bytes=index->Storage();
    // This proof cannot escape validation or survive a buffer write: the
    // local view is used only in this callback-free, read-only index loop.
    class VertexCoverage {
        buffers::VertexBinding binding_;
        std::size_t size_,maximum_index_;
        bool all_initialized_,invalid_binding_;
    public:
        explicit VertexCoverage(const buffers::VertexBinding& binding):binding_(binding),
            size_(binding.buffer?binding.buffer->Size():0),
            maximum_index_(binding.stride && binding.offset<=size_?(size_-binding.offset)/binding.stride:0),
            all_initialized_(binding.buffer && binding.buffer->Initialized(0,size_)),
            // This callback-free validation cannot change buffer/lock/stride.
            // Capture only the predicate; report it after each declared-index check.
            invalid_binding_(!binding.buffer || binding.buffer->Locked() || !binding.stride){}
        void Require(std::int64_t index)const{
            if(invalid_binding_ || index<0)
                throw std::out_of_range("staging vertex missing/negative");
            if(binding_.offset>size_ || std::uint64_t(index)>maximum_index_)
                throw std::out_of_range("staging vertex byte range");
            const auto offset=std::uint64_t(binding_.offset)+std::uint64_t(index)*binding_.stride;
            if(offset>size_ || (all_initialized_?binding_.stride>size_-offset:!binding_.buffer->Initialized(static_cast<std::size_t>(offset),binding_.stride)))
                throw std::out_of_range("staging draw references uninitialized vertex");
        }
    };
    const VertexCoverage coverage(binding);
    std::array<std::uint32_t,32> checked{};std::uint32_t occupied{};
    auto validate=[&]<class Index>(){
        for(std::uint64_t item=0;item<count;++item){
            Index raw;std::memcpy(&raw,bytes.data()+first+item*sizeof(Index),sizeof(Index));const std::uint32_t value=raw;
            if(value<minimum || std::uint64_t(value)-minimum>=vertices)throw std::out_of_range("staging index outside declared vertex range");
            const auto slot=value&31u,mask=std::uint32_t(1u)<<slot;
            if(!(occupied&mask) || checked[slot]!=value){coverage.Require(std::int64_t(base)+value);checked[slot]=value;occupied|=mask;}
        }
    };
    if(width==2)validate.template operator()<std::uint16_t>();else validate.template operator()<std::uint32_t>();
}
}

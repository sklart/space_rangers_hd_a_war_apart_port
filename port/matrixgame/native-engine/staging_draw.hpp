// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_resources.hpp"
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
        bool all_initialized_;
    public:
        explicit VertexCoverage(const buffers::VertexBinding& binding):binding_(binding),
            size_(binding.buffer?binding.buffer->Size():0),
            maximum_index_(binding.stride && binding.offset<=size_?(size_-binding.offset)/binding.stride:0),
            all_initialized_(binding.buffer && binding.buffer->Initialized(0,size_)){}
        void Require(std::int64_t index)const{
            if(!binding_.buffer || binding_.buffer->Locked() || !binding_.stride || index<0)
                throw std::out_of_range("staging vertex missing/negative");
            if(binding_.offset>size_ || std::uint64_t(index)>maximum_index_)
                throw std::out_of_range("staging vertex byte range");
            const auto offset=std::uint64_t(binding_.offset)+std::uint64_t(index)*binding_.stride;
            if(offset>size_ || (all_initialized_?binding_.stride>size_-offset:!binding_.buffer->Initialized(static_cast<std::size_t>(offset),binding_.stride)))
                throw std::out_of_range("staging draw references uninitialized vertex");
        }
    };
    const VertexCoverage coverage(binding);
    for(std::uint64_t item=0;item<count;++item){
        std::uint32_t value{};
        if(width==2){std::uint16_t short_value;std::memcpy(&short_value,bytes.data()+first+item*width,2);value=short_value;}
        else std::memcpy(&value,bytes.data()+first+item*width,4);
        if(value<minimum || std::uint64_t(value)-minimum>=vertices)throw std::out_of_range("staging index outside declared vertex range");
        coverage.Require(std::int64_t(base)+value);
    }
}
}

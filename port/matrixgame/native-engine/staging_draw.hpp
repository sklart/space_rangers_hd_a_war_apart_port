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
    for(std::uint64_t item=0;item<count;++item){
        std::uint32_t value{};
        if(width==2){std::uint16_t short_value;std::memcpy(&short_value,bytes.data()+first+item*width,2);value=short_value;}
        else std::memcpy(&value,bytes.data()+first+item*width,4);
        if(value<minimum || std::uint64_t(value)-minimum>=vertices)throw std::out_of_range("staging index outside declared vertex range");
        Vertex(binding,std::int64_t(base)+value);
    }
}
}

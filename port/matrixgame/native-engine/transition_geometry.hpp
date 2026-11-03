// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <string>
#include <stdexcept>
#include <type_traits>
namespace srhd_awa::matrixgame::transition {
template<class Block>struct GeometryLayout {Block* block;int width,height;float x_scale,y_scale;};
// Original resources contain these two authored layouts. At a native extent
// without an exact layout, scale existing geometry; never invent animation data
// or publish an empty transition. Exact authored resolutions remain unchanged.
template<class Lookup>auto ResolveGeometry(int width,int height,Lookup lookup){
    if(width<=0 || height<=0 || width>16384 || height>16384)throw std::invalid_argument("transition target extent");
    auto find=[&](int w,int h){return lookup(std::to_wstring(w)+L"."+std::to_wstring(h));};
    auto* exact=find(width,height);
    using Block=std::remove_pointer_t<decltype(exact)>;
    if(exact){if(exact->ParCount()<=0)throw std::runtime_error("empty authored transition geometry");return GeometryLayout<Block>{exact,width,height,1,1};}
    for(auto size:std::array<std::array<int,2>,2>{{{1024,768},{800,600}}}){
        auto* block=find(size[0],size[1]);if(!block)continue;
        if(block->ParCount()<=0)throw std::runtime_error("empty transition fallback geometry");
        return GeometryLayout<Block>{block,size[0],size[1],float(width)/size[0],float(height)/size[1]};
    }
    throw std::runtime_error("no authored transition geometry for native extent");
}
}

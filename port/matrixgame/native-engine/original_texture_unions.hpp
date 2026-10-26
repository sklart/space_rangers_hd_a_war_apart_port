// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdexcept>
#include <cstddef>
#include <string>
class CBitmap;
namespace Base {class CStorage;}
namespace srhd_awa::matrixgame {
// A resource view, not a replacement CMatrixMap. Production delegates directly
// to the original map IDs/progress; CPU checks supply the actual storage IDs.
struct TextureUnionInput {
    int dimension{},cells{},id_count{};
    const void* ids{};
    std::wstring (*read_id)(const void*,int){};
    void* progress{};
    void (*position)(void*,int){};
    void* observation{};
    void (*observe)(void*,int,const CBitmap&){};
    void (*decoded)(void*,bool,int,std::size_t){};
    void Decoded(bool mask,int id,const CBitmap& bitmap)const;
    std::wstring Id(int index)const{
        if(index<0 || index>=id_count || !read_id)throw std::out_of_range("original texture union string ID");return read_id(ids,index);
    }
    void SetPosition(int value)const{if(position)position(progress,value);}
};
// Original BuildTexUnions pixel loops with resource-view/RAII ownership and
// fail-closed input validation. Used by CMatrixMap::PrepareMap and CPU checks.
int BuildOriginalTextureUnions(Base::CStorage&,const TextureUnionInput&,int,int);
}

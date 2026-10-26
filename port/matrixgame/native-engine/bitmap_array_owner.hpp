// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CBitmap.hpp"
#include <span>
namespace srhd_awa::matrixgame {
// Own the original lazily decoded union bitmaps through exceptional exits too.
// Slots/lookup order and all original bitmap algorithms remain unchanged.
class BitmapArrayOwner {
    std::span<CBitmap*> slots_;
    Base::CHeap* heap_;
public:
    BitmapArrayOwner(std::span<CBitmap*> slots,Base::CHeap* heap)noexcept:slots_(slots),heap_(heap){}
    BitmapArrayOwner(const BitmapArrayOwner&)=delete;
    ~BitmapArrayOwner(){for(auto*& value:slots_)if(value){HDelete(CBitmap,value,heap_);value=nullptr;}}
};
}

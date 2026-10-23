// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
struct ID3DXFont;
namespace srhd_awa::matrixgame::debug_font {
struct TextRect {std::int32_t left,top,right,bottom;};
platform3g::DeviceResult CreateFor(platform3g::Device&,ID3DXFont**);
platform3g::DeviceResult Create(ID3DXFont**);
void Release(ID3DXFont*)noexcept;
int DrawTextWide(ID3DXFont*,const wchar_t*,int,TextRect&,std::uint32_t,std::uint32_t);
template<class Rect> int DrawTextWide(ID3DXFont* font,const wchar_t* text,int count,Rect* bounds,std::uint32_t flags,std::uint32_t color) {
    if(!bounds)throw std::invalid_argument("null debug font bounds");
    TextRect rect{bounds->left,bounds->top,bounds->right,bounds->bottom};
    const int result=DrawTextWide(font,text,count,rect,flags,color);
    bounds->left=rect.left;bounds->top=rect.top;bounds->right=rect.right;bounds->bottom=rect.bottom;
    return result;
}
platform3g::DeviceResult Lost(ID3DXFont*);
platform3g::DeviceResult Reset(ID3DXFont*);
platform3g::DeviceResult DrawPrimitiveUP(std::uint32_t,std::uint32_t,const void*,std::uint32_t);
}

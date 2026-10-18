// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace matrixgame::native {
inline constexpr std::size_t BitmapLimit=64*1024*1024, PngInputLimit=128*1024*1024;
inline int BitmapPitch(int w,int h,int bytes,int pitch=0) {
  if(w<=0||h<=0||w>8192||h>8192||bytes<1||bytes>4||pitch<0)
    throw std::invalid_argument("bitmap dimensions");
  const auto row=std::size_t(w)*bytes;
  const auto stride=pitch?std::size_t(pitch):row;
  if(stride<row||stride>BitmapLimit/std::size_t(h))throw std::length_error("bitmap extent");
  return static_cast<int>(stride);
}
inline bool BitmapRect(int x,int y,int w,int h,int width,int height) noexcept {
  return x>=0&&y>=0&&w>=0&&h>=0&&x<=width&&y<=height&&w<=width-x&&h<=height-y;
}
template<class T> T BitmapRead(const void* p) {T v;std::memcpy(&v,p,sizeof(v));return v;}
template<class T> void BitmapWrite(void* p,T v) {std::memcpy(p,&v,sizeof(v));}
}

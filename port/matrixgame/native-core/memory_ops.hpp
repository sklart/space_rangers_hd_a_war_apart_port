// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
namespace srhd_awa::matrixgame {
template<class T> void ZeroTrivialBytes(T* destination,std::size_t bytes) {
    static_assert(std::is_void_v<T> || std::is_trivially_copyable_v<T>,"nontrivial objects require explicit initialization");
    if(bytes && !destination)throw std::invalid_argument("null byte clear target");
    if(bytes)std::memset(destination,0,bytes);
}
struct RectI {std::int32_t x1,y1,x2,y2;};
static_assert(sizeof(RectI)==16);
}

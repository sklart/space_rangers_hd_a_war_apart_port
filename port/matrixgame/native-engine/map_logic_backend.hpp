// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "BaseDef.hpp"
#include <algorithm>

// Preserve caller-stack lifetime; an alloca wrapper function would return a
// pointer into its own dead frame. Match the original platform primitive.
#if defined(_MSC_VER)
#include <malloc.h>
#define MATRIXGAME_LOGIC_STACK_ALLOC(size) _alloca(size)
#else
#define MATRIXGAME_LOGIC_STACK_ALLOC(size) __builtin_alloca(size)
#endif

namespace srhd_awa::matrixgame::map_logic {
// Win32 UnionRect ignores empty inputs. Returning a value also preserves the
// original aliasing call UnionRect(&re, &re, &zone) without an SDK RECT cast.
inline Base::CRect UnionBounds(const Base::CRect& a, const Base::CRect& b) {
    if (a.IsEmpty()) return b.IsEmpty() ? Base::CRect(0,0,0,0) : b;
    if (b.IsEmpty()) return a;
    return {std::min(a.left,b.left),std::min(a.top,b.top),
            std::max(a.right,b.right),std::max(a.bottom,b.bottom)};
}
}

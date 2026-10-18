// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cfenv>
#include <exception>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

// These were compile-time constants in D3DX. Strict FP compilation must not
// turn their divisions into runtime operations under the toward-zero guard.
inline constexpr float BCInverse31 = 1.0f / 31.0f;
inline constexpr float BCInverse63 = 1.0f / 63.0f;
inline constexpr float BCInverse15 = 1.0f / 15.0f;
inline constexpr float BCInverse255 = 1.0f / 255.0f;

template<typename T> inline void BCRefreshForRounding(T& x,T& y) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    // GCC can reuse Y-X computed before fesetround when the Newton loop
    // repeats it under toward-zero rounding. A read/write compiler barrier
    // forces fresh operands after the mode change, without changing bytes.
    asm volatile("" : "+m"(x), "+m"(y) :: "memory");
#else
    (void)x;(void)y; // MSVC /fp:strict observes the rounding boundary.
#endif
}

// D3DX9 rounds floating-point arithmetic, not only integer conversions,
// toward zero in its quantization, Newton and index loops. Keep the change
// local to this thread and restore the caller's mode on every return.
class BCRoundToZero final {
    int saved_ = std::fegetround();
    bool changed_ = saved_ != FE_TOWARDZERO;
public:
    BCRoundToZero() noexcept {
        if (saved_ == -1 || (changed_ && std::fesetround(FE_TOWARDZERO)))
            std::terminate();
    }
    ~BCRoundToZero() noexcept { Restore(); }
    void Restore() noexcept {
        if (changed_) {
            if (std::fesetround(saved_)) std::terminate();
            changed_ = false;
        }
    }
    BCRoundToZero(const BCRoundToZero&) = delete;
    BCRoundToZero& operator=(const BCRoundToZero&) = delete;
};

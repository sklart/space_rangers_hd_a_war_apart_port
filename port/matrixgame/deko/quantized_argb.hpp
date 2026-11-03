// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "material_combiner.hpp"
#include <bit>
#include <limits>
namespace srhd_awa::matrixgame::materials {
// Only ARGB8-derived colors (or zero/one defaults), not a general quantizer.
// Retain the original float multiply and its FP flags. Round the resulting
// nonnegative float using the original AArch64 SDK instruction, or integer
// bits on x64. Preserve each platform reference conversion FP behavior.
inline std::uint32_t PackQuantizedArgb(const Color& c)noexcept{
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
    auto byte=[](float v){
#if defined(__aarch64__)
        const float scaled=v*255.f;
        std::int64_t rounded;
        // The SDK lroundf body is FCVTAS x0,s0; RET. Inline the same conversion
        // (including FP status) instead of calling it. Volatile retains it even
        // when a validation-only caller discards this logical word's value.
        asm volatile("fcvtas %x0, %s1" : "=r"(rounded) : "w"(scaled));
        return std::uint32_t(rounded);
#else
        const auto bits=std::bit_cast<std::uint32_t>(v*255.f);
        if(!(bits&0x7fffffffu))return 0u;
        // Nonzero ARGB8-derived scaled components are near integers 1..255,
        // so shift is 16..24. Zero was handled above; no invalid shifts.
        const auto shift=150u-((bits>>23)&255u);
        return (((bits&0x7fffffu)|0x800000u)+(1u<<(shift-1)))>>shift;
#endif
    };
    return (byte(c[3])<<24)|(byte(c[0])<<16)|(byte(c[1])<<8)|byte(c[2]);
}
}

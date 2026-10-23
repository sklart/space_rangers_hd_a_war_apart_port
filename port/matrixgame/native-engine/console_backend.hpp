// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
namespace srhd_awa::matrixgame::console {
// Original Windows virtual-key protocol, independent of physical HID mapping.
inline constexpr std::uint8_t Back=0x08,Delete=0x2e,Left=0x25,Right=0x27,
    Home=0x24,End=0x23,Return=0x0d,Escape=0x1b,Shift=0x10;
// Only the TRACESPD developer command measures an elapsed interval with this
// clock. Keep unsigned-32 wrap semantics; no game/loop timer uses this adapter.
inline std::uint32_t BenchmarkMilliseconds() {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

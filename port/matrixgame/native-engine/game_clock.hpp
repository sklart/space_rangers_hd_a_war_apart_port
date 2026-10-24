// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
namespace srhd_awa::matrixgame {
// One monotonic modulo-2^32 millisecond domain for both map start timestamps
// and side STAT_TIME. This does not schedule the game loop or advance logic.
inline std::uint32_t GameClockMilliseconds() {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

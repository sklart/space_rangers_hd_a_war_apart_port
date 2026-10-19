#pragma once
#include <algorithm>
#include <cstdint>

namespace srhd_awa::platform {
// Carry includes the previous frame's simulation cost. Arcade callbacks must
// receive all elapsed milliseconds; clamping either term slows world time.
// The film player retains its original long-frame policy.
inline std::uint32_t ContinuousTimerMillis(std::uint32_t frame_ms,
                                         std::uint32_t carry_ms, bool arcade) {
    if (!arcade) {
        frame_ms = std::min(frame_ms, 200u);
        if (carry_ms > 200u) carry_ms = 0;
    }
    return frame_ms + carry_ms;
}
}  // namespace srhd_awa::platform

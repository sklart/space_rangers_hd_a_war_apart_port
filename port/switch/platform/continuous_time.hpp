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

// A slow callback must not trap the UI in an ever-growing catch-up batch.
// Debt is retained across presentations; no logical millisecond is discarded.
class ContinuousTimerDebt {
public:
    void Add(std::uint32_t frame_ms, std::uint32_t carry_ms) {
        pending_ += std::uint64_t(frame_ms) + carry_ms;
    }
    bool TakeOne() {
        if (!pending_) return false;
        --pending_;
        return true;
    }
    std::uint64_t Pending() const { return pending_; }
    static constexpr std::uint32_t BatchBudgetMs = 8;
private:
    std::uint64_t pending_{};
};

inline std::uint32_t WithoutNestedLoopTime(std::uint32_t elapsed_ms,
                                         std::uint64_t nested_before,
                                         std::uint64_t nested_after) {
    const auto paused = nested_after - nested_before;
    return elapsed_ms - static_cast<std::uint32_t>(std::min<std::uint64_t>(elapsed_ms, paused));
}
}  // namespace srhd_awa::platform

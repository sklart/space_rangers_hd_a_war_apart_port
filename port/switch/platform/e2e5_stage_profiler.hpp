#pragma once

#include "e2e_stage.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace srhd_awa::platform::e2e5_stage_profiler {

struct UiWork {
    std::uint64_t queued_rects{};
    std::uint64_t queued_area_px{};
    std::uint64_t root_draw_calls{};
    std::uint64_t control_visits{};
    std::uint64_t framebuffer_bytes{};
    std::uint64_t presentations{};
};

// The original UI runs on one game thread. These counters share the existing
// five-second stage window and have no per-control logging or allocation.
inline UiWork ui_work{};

inline void QueueRect(std::int32_t left, std::int32_t top,
                      std::int32_t right, std::int32_t bottom) {
    ++ui_work.queued_rects;
    const auto width = std::max<std::int64_t>(0, static_cast<std::int64_t>(right) - left);
    const auto height = std::max<std::int64_t>(0, static_cast<std::int64_t>(bottom) - top);
    ui_work.queued_area_px += static_cast<std::uint64_t>(width * height);
}

inline void PresentFrame(std::int32_t width, std::int32_t height) {
    if (width > 0 && height > 0) {
        ui_work.framebuffer_bytes += static_cast<std::uint64_t>(width) * height * 2u;
        ++ui_work.presentations;
    }
}

// Each original GI_MessageLoop::Run Stage assignment closes the preceding
// interval.  A bounded sample retains percentile evidence without per-frame
// allocation.  Numeric stage IDs map directly to the original source.
class Window {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t kStages = 40;
    static constexpr std::size_t kSamples = 128;

    void Enter(std::uint32_t next) {
        const auto now = Clock::now();
        if (active_ < kStages) {
            const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - last_).count();
            Record(active_, delta > 0 ? static_cast<std::uint64_t>(delta) : 0);
        }
        active_ = next;
        last_ = now;
        if (now - window_start_ >= std::chrono::seconds(5)) {
            Flush();
            window_start_ = now;
        }
    }

    void Stop() {
        Enter(kStages);
    }

    void Flush() {
        for (std::size_t stage = 0; stage < stages_.size(); ++stage) {
            auto &stats = stages_[stage];
            if (!stats.count) continue;
            std::sort(stats.samples.begin(), stats.samples.begin() + stats.sample_count);
            auto percentile = [&](std::uint32_t numerator) -> std::uint32_t {
                const auto index = ((stats.sample_count - 1) * numerator + 99) / 100;
                return stats.samples[index];
            };
            char line[192]{};
            std::snprintf(line, sizeof(line),
                "perf stage=%u count=%llu avg_us=%llu p50_us=%u p95_us=%u p99_us=%u max_us=%u sample=%u",
                static_cast<unsigned>(stage),
                static_cast<unsigned long long>(stats.count),
                static_cast<unsigned long long>(stats.sum / stats.count),
                percentile(50), percentile(95), percentile(99), stats.maximum,
                stats.sample_count);
            e2e_stage::Log(line);
            stats = {};
        }
        if (ui_work.presentations || ui_work.queued_rects || ui_work.control_visits) {
            char line[256]{};
            std::snprintf(line, sizeof(line),
                "perf ui presents=%llu rects=%llu rect_area_px=%llu root_draw_calls=%llu control_visits=%llu framebuffer_bytes=%llu",
                static_cast<unsigned long long>(ui_work.presentations),
                static_cast<unsigned long long>(ui_work.queued_rects),
                static_cast<unsigned long long>(ui_work.queued_area_px),
                static_cast<unsigned long long>(ui_work.root_draw_calls),
                static_cast<unsigned long long>(ui_work.control_visits),
                static_cast<unsigned long long>(ui_work.framebuffer_bytes));
            e2e_stage::Log(line);
            ui_work = {};
        }
    }

private:
    struct Stats {
        std::array<std::uint32_t, kSamples> samples{};
        std::uint64_t count{};
        std::uint64_t sum{};
        std::uint32_t maximum{};
        std::uint32_t sample_count{};
    };
    std::array<Stats, kStages> stages_{};
    Clock::time_point window_start_{Clock::now()};
    Clock::time_point last_{Clock::now()};
    std::uint32_t active_{kStages};

    void Record(std::uint32_t stage, std::uint64_t microseconds) {
        auto &stats = stages_[stage];
        const auto bounded = static_cast<std::uint32_t>(std::min<std::uint64_t>(microseconds, std::numeric_limits<std::uint32_t>::max()));
        stats.sum += microseconds;
        ++stats.count;
        stats.maximum = std::max(stats.maximum, bounded);
        if (stats.sample_count < kSamples) {
            stats.samples[stats.sample_count++] = bounded;
        } else {
            // Deterministic bounded down-sampling, including late frames.
            stats.samples[(stats.count - 1) % kSamples] = bounded;
        }
    }
};

}  // namespace srhd_awa::platform::e2e5_stage_profiler

#pragma once

#include "e2e_stage.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include "e2e_clock.hpp"

namespace srhd_awa::platform::e2e5_stage_profiler {

struct UiWork {
    std::uint64_t queued_rects{};
    std::uint64_t queued_area_px{};
    std::uint64_t root_draw_calls{};
    std::uint64_t control_visits{};
    std::uint64_t framebuffer_bytes{};
    std::uint64_t presentations{};
    std::uint64_t intro_draws{}, intro_applied{}, intro_presented{}, intro_repeated{}, intro_skipped{};
};

// The original UI runs on one game thread. These counters share the existing
// five-second stage window and have no per-control logging or allocation.
inline UiWork ui_work{};
inline std::int32_t current_screen{-1};
inline std::int32_t intro_position{-1}, intro_presented_position{-1};
inline bool intro_pending{};

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
        if (intro_pending && intro_position >= 0) {
            ++ui_work.intro_presented;
            if (intro_position == intro_presented_position) ++ui_work.intro_repeated;
            if (intro_presented_position >= 0 && intro_position > intro_presented_position + 1)
                ui_work.intro_skipped += intro_position - intro_presented_position - 1;
            intro_presented_position = intro_position;
            intro_pending = false;
        }
    }
}

inline void FlushUiWork() {
        if (ui_work.presentations || ui_work.queued_rects || ui_work.control_visits) {
            char line[256]{};
            std::snprintf(line, sizeof(line),
                "perf ui screen=%d presents=%llu rects=%llu rect_area_px=%llu root_draw_calls=%llu control_visits=%llu framebuffer_bytes=%llu",
                current_screen,
                static_cast<unsigned long long>(ui_work.presentations),
                static_cast<unsigned long long>(ui_work.queued_rects),
                static_cast<unsigned long long>(ui_work.queued_area_px),
                static_cast<unsigned long long>(ui_work.root_draw_calls),
                static_cast<unsigned long long>(ui_work.control_visits),
                static_cast<unsigned long long>(ui_work.framebuffer_bytes));
            e2e_stage::Log(line);
            if (ui_work.intro_draws || ui_work.intro_presented) {
                char intro[192]{};
                std::snprintf(intro, sizeof(intro),
                    "perf intro draws=%llu applied=%llu presented=%llu repeated=%llu skipped_positions=%llu position=%d",
                    static_cast<unsigned long long>(ui_work.intro_draws),
                    static_cast<unsigned long long>(ui_work.intro_applied),
                    static_cast<unsigned long long>(ui_work.intro_presented),
                    static_cast<unsigned long long>(ui_work.intro_repeated),
                    static_cast<unsigned long long>(ui_work.intro_skipped), intro_position);
                e2e_stage::Log(intro);
            }
            ui_work = {};
        }
}

inline void SetScreen(std::int32_t screen) {
    if (screen != current_screen) {
        FlushUiWork();
        intro_pending = false;
        current_screen = screen;
    }
}

// Each original GI_MessageLoop::Run Stage assignment closes the preceding
// interval.  A bounded sample retains percentile evidence without per-frame
// allocation.  Numeric stage IDs map directly to the original source.
class Window {
public:
    struct Clock {
        using duration=std::chrono::microseconds;
        using time_point=std::chrono::time_point<Clock,duration>;
        static time_point now(){return time_point(duration(e2e_clock::Microseconds()));}
    };
    static constexpr std::size_t kStages = 48;
    static constexpr std::size_t kSamples = 128;
    explicit Window(const char* domain = "loop") : domain_(domain) {}

    ~Window() { Stop(); Flush(); }

    void Measure(std::uint32_t stage, std::uint64_t microseconds) {
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
        if(!perf515::Mode())return;
        if(std::strcmp(domain_,"present")==0) {
            static constexpr const char* names[]={"prepare","upload","copy","present","presentation_total","frame_interval"};
            if(stage>=40&&stage<=45)perf515::Record(names[stage-40],microseconds);
        }
        if(std::strcmp(domain_,"arcade-phase")==0&&stage==2)
            perf515::Record("advance_total",microseconds);
#endif
        if (screen_ != current_screen) {
            Flush();
            screen_ = current_screen;
            window_start_ = Clock::now();
        }
        if (stage < kStages) Record(stage, microseconds);
        const auto now = Clock::now();
        if (now - window_start_ >= std::chrono::seconds(5)) {
            Flush();
            window_start_ = now;
        }
    }

    void Enter(std::uint32_t next) {
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
        if(!perf515::Mode())return;
#endif
        const auto now = Clock::now();
        if (active_ < kStages) {
            const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - last_).count();
            Record(active_, delta > 0 ? static_cast<std::uint64_t>(delta) : 0);
        }
        if (screen_ != current_screen) {
            Flush();
            screen_ = current_screen;
        }
        active_ = next;
        if (now - window_start_ >= std::chrono::seconds(5)) {
            Flush();
            window_start_ = now;
        }
        // Do not charge synchronous diagnostic publication to the next stage.
        last_ = Clock::now();
    }

    void Stop() {
        Enter(kStages);
    }

    void Flush() {
        bool populated = false;
        for (std::size_t stage = 0; stage < stages_.size(); ++stage) {
            auto &stats = stages_[stage];
            if (!stats.count) continue;
            populated = true;
            std::sort(stats.samples.begin(), stats.samples.begin() + stats.sample_count);
            auto percentile = [&](std::uint32_t numerator) -> std::uint32_t {
                const auto index = ((stats.sample_count - 1) * numerator + 99) / 100;
                return stats.samples[index];
            };
            char line[192]{};
            std::snprintf(line, sizeof(line),
                "perf screen=%d source=%s stage=%u count=%llu avg_us=%llu p50_us=%u p95_us=%u p99_us=%u max_us=%u sample=%u",
                screen_, domain_,
                static_cast<unsigned>(stage),
                static_cast<unsigned long long>(stats.count),
                static_cast<unsigned long long>(stats.sum / stats.count),
                percentile(50), percentile(95), percentile(99), stats.maximum,
                stats.sample_count);
            e2e_stage::Log(line);
            stats = {};
        }
        FlushUiWork();
        if (populated) {
            char audio[256]{};
            std::snprintf(audio, sizeof(audio),
                "perf audio cumulative source_calls=%llu source_us=%llu source_max_us=%llu decode_calls=%llu decode_inclusive_us=%llu decode_max_us=%llu",
                static_cast<unsigned long long>(e2e_stage::audio_source.calls.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(e2e_stage::audio_source.total_us.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(e2e_stage::audio_source.max_us.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(e2e_stage::audio_decode.calls.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(e2e_stage::audio_decode.total_us.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(e2e_stage::audio_decode.max_us.load(std::memory_order_relaxed)));
            e2e_stage::Log(audio);
            const auto stats = e2e_stage::Stats();
            char line[192]{};
            std::snprintf(line, sizeof(line),
                "perf log cumulative calls=%llu write_us=%llu flushes=%llu flush_us=%llu",
                static_cast<unsigned long long>(stats.calls),
                static_cast<unsigned long long>(stats.write_us),
                static_cast<unsigned long long>(stats.flushes),
                static_cast<unsigned long long>(stats.flush_us));
            e2e_stage::Log(line);
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
    std::int32_t screen_{current_screen};
    const char* domain_;

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

inline Window detail_window{"detail"};
struct Scope {
    Window& window;
    std::uint32_t stage;
    bool enabled;
    Window::Clock::time_point start;
    Scope(Window& target, std::uint32_t id, bool active = true)
        : window(target), stage(id), enabled(active), start(active ? Window::Clock::now() : Window::Clock::time_point{}) {}
    ~Scope() {
        if (enabled) window.Measure(stage, std::chrono::duration_cast<std::chrono::microseconds>(
            Window::Clock::now() - start).count());
    }
};

}  // namespace srhd_awa::platform::e2e5_stage_profiler

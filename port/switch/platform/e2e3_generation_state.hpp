#pragma once

#include <atomic>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "e2e_stage.hpp"

namespace srhd_awa::platform::e2e3_generation_state {

// The original worker and Introduction UI run on different threads. Keep the
// original 0..8 progress semantics while publishing them without a data race.
inline std::atomic<std::int32_t> g_stage{0};
#if defined(E2E5_HOST_SDL_WINDOW)
inline std::atomic<bool> g_host_continue_ready{false};
#endif
inline std::atomic<std::int32_t> g_failure_stage{-1};
inline std::atomic<std::int64_t> g_stage_start_us{0};
inline std::atomic<std::int64_t> g_generation_start_us{0};
// Encoded as percent+1, negative attempt number, or zero when no denominator
// is known. Separate stages retain 100% after the worker advances.
inline std::array<std::atomic<int>, 8> g_progress{};
inline void Work(int stage, int completed, int total) {
  if (stage < 0 || stage >= 8 || total <= 0) return;
  const auto percent = 100LL * std::clamp(completed, 0, total) / total;
  g_progress[stage].store(static_cast<int>(percent) + 1, std::memory_order_relaxed);
}
inline void Attempt(int stage, int attempt) {
  if (stage >= 0 && stage < 8)
    g_progress[stage].store(-std::max(1, attempt), std::memory_order_relaxed);
}
inline int Progress(int stage) {
  return stage >= 0 && stage < 8 ? g_progress[stage].load(std::memory_order_relaxed) : 0;
}

inline std::int64_t ClockUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void Reset() {
#if defined(E2E5_HOST_SDL_WINDOW)
  g_host_continue_ready.store(false);
#endif
  const auto now = ClockUs();
  g_generation_start_us.store(now, std::memory_order_release);
  g_stage_start_us.store(now, std::memory_order_release);
  g_failure_stage.store(-1, std::memory_order_release);
  g_stage.store(0, std::memory_order_release);
  for (auto& progress : g_progress) progress.store(0, std::memory_order_relaxed);
  e2e_stage::LogE2E3("generation status=starting");
}

inline void SetStage(std::int32_t stage) {
  const auto now = ClockUs();
  const auto previous = g_stage.exchange(stage, std::memory_order_acq_rel);
  if (previous != stage) {
    if (previous >= 0 && previous < 8 && stage > previous)
      g_progress[previous].store(101, std::memory_order_relaxed);
    const auto prior_start = g_stage_start_us.exchange(now, std::memory_order_acq_rel);
    const auto origin = g_generation_start_us.load(std::memory_order_acquire);
    char detail[128]{};
    std::snprintf(detail, sizeof(detail),
                  "generation stage=%d status=%s previous_stage=%d previous_ms=%lld total_ms=%lld",
                  stage, stage == 8 ? "complete" : "running", previous,
                  static_cast<long long>(prior_start > 0 ? (now - prior_start) / 1000 : 0),
                  static_cast<long long>(origin > 0 ? (now - origin) / 1000 : 0));
    e2e_stage::LogE2E3(detail);
  }
}

inline std::int32_t Stage() {
  return g_stage.load(std::memory_order_acquire);
}

inline void Fail(std::int32_t at_stage) {
  g_failure_stage.store(at_stage, std::memory_order_release);
  char detail[72]{};
  std::snprintf(detail, sizeof(detail), "generation stage=%d status=failed", at_stage);
  e2e_stage::LogE2E3(detail);
}

inline bool Failed() {
  return g_failure_stage.load(std::memory_order_acquire) >= 0;
}

inline std::int32_t FailureStage() {
  return g_failure_stage.load(std::memory_order_acquire);
}

}  // namespace srhd_awa::platform::e2e3_generation_state

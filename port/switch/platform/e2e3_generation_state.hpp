#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "e2e_stage.hpp"

namespace srhd_awa::platform::e2e3_generation_state {

// The original worker and Introduction UI run on different threads. Keep the
// original 0..8 progress semantics while publishing them without a data race.
inline std::atomic<std::int32_t> g_stage{0};
inline std::atomic<std::int32_t> g_failure_stage{-1};

inline void Reset() {
  g_failure_stage.store(-1, std::memory_order_release);
  g_stage.store(0, std::memory_order_release);
  e2e_stage::LogE2E3("generation status=starting");
}

inline void SetStage(std::int32_t stage) {
  const auto previous = g_stage.exchange(stage, std::memory_order_acq_rel);
  if (previous != stage) {
    char detail[64]{};
    std::snprintf(detail, sizeof(detail), "generation stage=%d status=%s",
                  stage, stage == 8 ? "complete" : "running");
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

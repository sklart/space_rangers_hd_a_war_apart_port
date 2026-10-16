#pragma once
#include "e2e3_generation_state.hpp"
#include "e2e5_failure_diagnostics.hpp"
#include "e2e_clock.hpp"
#include <array>
#include <atomic>

namespace srhd_awa::platform::e2e58_generation_profile {
struct Counters {
  std::atomic<std::uint64_t> calls{}, samples{}, microseconds{};
};
inline std::array<Counters, 6> counters;
inline constexpr const char* names[] = {"star-day", "star-prepare", "ship-day",
                                       "ship-logic", "equipment", "bonus"};
class Scope {
  unsigned id_;
  bool sampled_{};
  std::uint64_t began_{};
public:
  explicit Scope(unsigned id) : id_(id) {
    if (e2e3_generation_state::Stage() != 6) return;
    sampled_ = (counters[id].calls.fetch_add(1, std::memory_order_relaxed) & 63u) == 0;
    if (sampled_) began_ = e2e_clock::Microseconds();
  }
  ~Scope() noexcept {
    if (!sampled_) return;
    counters[id_].microseconds.fetch_add(e2e_clock::Microseconds() - began_, std::memory_order_relaxed);
    counters[id_].samples.fetch_add(1, std::memory_order_relaxed);
  }
};
inline void Flush() noexcept {
  // Cumulative atomics avoid cross-thread window reset races. Inclusive times
  // are sampled every 64 calls; they must not be summed or treated as totals.
  for (unsigned i = 0; i < counters.size(); ++i) {
    auto& c = counters[i];
    char line[192]{};
    std::snprintf(line, sizeof(line),
        "generation function=%s calls=%llu sampled=%llu sampled_inclusive_us=%llu stride=64",
        names[i], static_cast<unsigned long long>(c.calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(c.samples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(c.microseconds.load(std::memory_order_relaxed)));
    e2e5_failure_diagnostics::Write(line);
  }
}
}

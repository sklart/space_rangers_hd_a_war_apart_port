#pragma once
#include "e2e_clock.hpp"
#include "e2e5_failure_diagnostics.hpp"
#include "units/GlobalsV.hpp"
#include <cstdio>

namespace srhd_awa::platform {
class TurnProfile {
  const char* operation_;
  std::uint64_t began_;
  int screen_;
public:
  explicit TurnProfile(const char* operation) : operation_(operation),
      began_(e2e_clock::Microseconds()), screen_(GlobalsV::CurrentScreenId) {}
  ~TurnProfile() noexcept {
    if (screen_ == 3) return; // Intro generation has its own sampled profiler.
    char line[160]{};
    std::snprintf(line, sizeof(line), "turn operation=%s screen=%d elapsed_us=%llu",
        operation_, screen_, static_cast<unsigned long long>(e2e_clock::Microseconds() - began_));
    e2e5_failure_diagnostics::Write(line);
  }
};
}

#pragma once

#include "runtime_platform.hpp"

#include <cstdint>
#include <string>

namespace srhd_awa::platform::runtime_loop_slice {

enum class ExitReason { none, requested, plus, applet, presentation_failure };

struct Statistics {
  std::uint64_t frames{};
  std::uint64_t presents{};
  std::uint64_t duration_ms{};
  ExitReason exit_reason{ExitReason::none};
};

struct State {
  bool initialized{};
  bool exit_requested{};
  std::uint64_t started_tick{};
  Statistics statistics{};
};

bool Initialize(State* state, std::string* error);
bool RunFrames(State* state, const runtime_platform::State& platform, std::uint64_t frame_count,
               std::string* error);
bool RunPersistent(State* state, const runtime_platform::State& platform, std::string* error);
void RequestExit(State* state);
void Shutdown(State* state);
const char* ExitReasonName(ExitReason reason);

}  // namespace srhd_awa::platform::runtime_loop_slice
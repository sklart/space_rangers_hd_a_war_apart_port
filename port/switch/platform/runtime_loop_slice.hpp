#pragma once

#include "runtime_platform.hpp"

#include <cstdint>
#include <string>

namespace srhd_awa::platform::runtime_loop_slice {

enum class ExitReason { none, requested, plus, applet, presentation_failure, diagnostic_failure };

using FrameCallback = bool (*)(void* user_data, std::uint64_t now_ms, std::string* error);
using DrawCallback = bool (*)(void* user_data, std::string* error);

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
  FrameCallback frame_callback{};
  void* frame_callback_user{};
  DrawCallback draw_callback{};
  void* draw_callback_user{};
  Statistics statistics{};
};

bool Initialize(State* state, std::string* error);
void SetFrameCallback(State* state, FrameCallback callback, void* user_data);
void SetDrawCallback(State* state, DrawCallback callback, void* user_data);
bool RunFrames(State* state, const runtime_platform::State& platform, std::uint64_t frame_count,
               std::string* error);
bool RunPersistent(State* state, const runtime_platform::State& platform, std::string* error);
void RequestExit(State* state);
void Shutdown(State* state);
const char* ExitReasonName(ExitReason reason);

}  // namespace srhd_awa::platform::runtime_loop_slice

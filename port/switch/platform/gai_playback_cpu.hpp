#pragma once

#include "gai_cpu.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::gai_playback_cpu {

struct State {
  bool initialized{};
  bool running{};
  bool stop_after_one_cycle{};
  std::int32_t sequence_index{};
  std::int32_t sequence_frame{};
  std::uint64_t elapsed_in_frame_ms{};
  std::uint64_t cycles_completed{};
};

struct Step {
  std::int32_t sequence_frame{};
  std::int32_t source_frame_index{};
  std::int32_t delay_ms{};
  bool wrapped{};
};

bool Initialize(State* state, const gai_cpu::GaiSequence& sequence, std::string* error = nullptr);
bool AdvanceBy(State* state, const gai_cpu::GaiSequence& sequence, std::uint64_t elapsed_ms,
               std::vector<Step>* entered_frames, std::string* error = nullptr);
bool Stop(State* state, std::string* error = nullptr);
bool Restart(State* state, const gai_cpu::GaiSequence& sequence, std::string* error = nullptr);
bool SetFramePosition(State* state, const gai_cpu::GaiSequence& sequence, std::int32_t frame,
                      bool forward_only, std::string* error = nullptr);

}  // namespace srhd_awa::platform::gai_playback_cpu

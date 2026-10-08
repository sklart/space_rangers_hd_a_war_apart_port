#include "gai_playback_cpu.hpp"

#include <limits>

namespace srhd_awa::platform::gai_playback_cpu {
namespace {
constexpr std::uint64_t kMaxTransitions = 100000;
bool Valid(const gai_cpu::GaiSequence& sequence, std::string* error) {
  if (sequence.frames.empty()) { if (error) *error = "empty sequence"; return false; }
  for (const auto& frame : sequence.frames) {
    if (frame.delay_ms <= 0) { if (error) *error = "non-positive delay"; return false; }
  }
  return true;
}
bool Ready(const State* state, const gai_cpu::GaiSequence& sequence, std::string* error) {
  if (!state || !state->initialized || !Valid(sequence, error) || state->sequence_frame < 0 ||
      state->sequence_frame >= static_cast<std::int32_t>(sequence.frames.size())) {
    if (error && error->empty()) *error = "state";
    return false;
  }
  return true;
}
}  // namespace

bool Initialize(State* state, const gai_cpu::GaiSequence& sequence, std::string* error) {
  if (error) error->clear();
  if (!state || !Valid(sequence, error)) return false;
  *state = {};
  state->initialized = true;
  state->running = true;
  state->sequence_index = sequence.index;
  return true;
}

bool AdvanceBy(State* state, const gai_cpu::GaiSequence& sequence, std::uint64_t elapsed_ms,
               std::vector<Step>* entered_frames, std::string* error) {
  if (entered_frames) entered_frames->clear();
  if (error) error->clear();
  if (!Ready(state, sequence, error)) return false;
  if (!state->running || sequence.frames.size() == 1) return true;
  if (elapsed_ms > std::numeric_limits<std::uint64_t>::max() - state->elapsed_in_frame_ms) {
    if (error) *error = "elapsed overflow";
    return false;
  }
  state->elapsed_in_frame_ms += elapsed_ms;
  for (std::uint64_t transitions = 0;; ++transitions) {
    if (transitions >= kMaxTransitions) { if (error) *error = "transition limit"; return false; }
    const auto delay = static_cast<std::uint64_t>(sequence.frames[state->sequence_frame].delay_ms);
    if (state->elapsed_in_frame_ms < delay) return true;
    state->elapsed_in_frame_ms -= delay;
    ++state->sequence_frame;
    bool wrapped = false;
    if (state->sequence_frame == static_cast<std::int32_t>(sequence.frames.size())) {
      state->sequence_frame = 0;
      ++state->cycles_completed;
      wrapped = true;
    }
    const auto& frame = sequence.frames[state->sequence_frame];
    if (entered_frames) entered_frames->push_back({state->sequence_frame, frame.source_frame_index, frame.delay_ms, wrapped});
    if (wrapped && state->stop_after_one_cycle) { state->running = false; return true; }
  }
}

bool Stop(State* state, std::string* error) {
  if (error) error->clear();
  if (!state || !state->initialized) { if (error) *error = "state"; return false; }
  state->running = false;
  return true;
}

bool Restart(State* state, const gai_cpu::GaiSequence& sequence, std::string* error) {
  if (error) error->clear();
  if (!Ready(state, sequence, error)) return false;
  state->running = true;
  state->elapsed_in_frame_ms = 0;
  return true;
}

bool SetFramePosition(State* state, const gai_cpu::GaiSequence& sequence, std::int32_t frame,
                      bool forward_only, std::string* error) {
  if (error) error->clear();
  if (!Ready(state, sequence, error)) return false;
  if (frame == state->sequence_frame || (forward_only && frame <= state->sequence_frame)) return true;
  if (frame < 0 || frame >= static_cast<std::int32_t>(sequence.frames.size())) frame = 0;
  state->sequence_frame = frame;
  state->elapsed_in_frame_ms = 0;
  return true;
}

}  // namespace srhd_awa::platform::gai_playback_cpu

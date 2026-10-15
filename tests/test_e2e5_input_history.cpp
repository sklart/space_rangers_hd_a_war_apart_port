#include "input_platform.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using namespace srhd_awa::platform::input_platform;

namespace {
constexpr std::uint64_t kSampleUs = 16667;

void CheckPulse(int fps, int duration_ms, int phase_ms) {
  State state;
  std::vector<RawHistoryEntry> samples;
  int downs = 0, ups = 0;
  const auto start = static_cast<std::uint64_t>(phase_ms) * 1000;
  const auto end = start + static_cast<std::uint64_t>(duration_ms) * 1000;
  const auto frame_us = static_cast<std::uint64_t>(1000000 / fps);
  std::uint64_t next_sequence = 0;
  for (std::uint64_t poll_us = 0; poll_us <= 1500000; poll_us += frame_us) {
    while (next_sequence * kSampleUs <= poll_us) {
      const auto sample_us = next_sequence * kSampleUs;
      RawInput raw{};
      raw.a = sample_us >= start && sample_us < end;
      samples.push_back({next_sequence, raw});
      ++next_sequence;
    }
    const auto first = samples.size() > 17 ? samples.size() - 17 : 0;
    const auto batch = NormalizeHistoryAt(&state, samples.data() + first,
        samples.size() - first, samples.back().raw, 1280, 720, poll_us);
    for (std::size_t index = 0; index < batch.count; ++index) {
      downs += batch.snapshots[index].left_down;
      ups += batch.snapshots[index].left_up;
    }
  }
  assert(downs == 1 && ups == 1);
}
}  // namespace

int main() {
  for (const int fps : {5, 10, 15, 30, 60}) {
    for (const int duration : {30, 50, 100, 150, 200, 300}) {
      const int frame_ms = 1000 / fps;
      for (const int phase : {1, frame_ms / 2, frame_ms - 1})
        CheckPulse(fps, duration, phase);
    }
  }

  State moving;
  RawInput raw{};
  raw.stick_x = 32767;
  RawHistoryEntry initial{0, raw};
  NormalizeHistoryAt(&moving, &initial, 1, raw, 1280, 720, 0);
  std::array<RawHistoryEntry, 12> history{};
  for (std::size_t i = 0; i < history.size(); ++i) {
    history[i] = {i + 1, raw};
    history[i].raw.a = i >= 4 && i < 8;
  }
  const auto batch = NormalizeHistoryAt(&moving, history.data(), history.size(),
                                        raw, 1280, 720, 200000);
  int down_x = -1, up_x = -1;
  for (std::size_t i = 0; i < batch.count; ++i) {
    if (batch.snapshots[i].left_down) down_x = batch.snapshots[i].x;
    if (batch.snapshots[i].left_up) up_x = batch.snapshots[i].x;
  }
  assert(down_x >= 640 && up_x > down_x);

  State stalled;
  RawHistoryEntry first{0, {}};
  NormalizeHistoryAt(&stalled, &first, 1, {}, 1280, 720, 0);
  std::array<RawHistoryEntry, 17> late{};
  for (std::size_t i = 0; i < late.size(); ++i)
    late[i].sequence = 44 + i;
  const auto after_stall = NormalizeHistoryAt(&stalled, late.data(), late.size(),
                                              {}, 1280, 720, 1000000);
  assert(after_stall.missed_samples == 43);
  for (std::size_t i = 0; i < after_stall.count; ++i)
    assert(!after_stall.snapshots[i].left_down && !after_stall.snapshots[i].left_up);
}

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace srhd_awa::platform::input_platform {

struct Config {
  std::int32_t stick_deadzone{5500};
  double stick_pixels_per_second{600.0};
  double dpad_pixels_per_second{180.0};
  std::uint64_t max_delta_us{50000};  // Motion after an actual stall.
  std::uint64_t stall_threshold_us{500000};
};
struct RawInput {
  std::int32_t stick_x{}, stick_y{};
  bool dpad_left{}, dpad_right{}, dpad_up{}, dpad_down{};
  bool a{}, b{}, x{}, y{}, plus{};
  bool zl{}, zr{}, minus{};
  bool touch{};
  std::int32_t touch_x{}, touch_y{}; // HID coordinates in the 1280x720 panel.
  std::int32_t right_stick_x{}, right_stick_y{};
};
struct Snapshot {
  std::uint8_t flight_keys{}; // up/down/left/right/primary/secondary, arcade only
  std::int32_t x{}, y{};
  std::uint64_t hid_sequence{};
  bool left_down{}, left_up{}, left_held{};
  bool right_down{}, right_up{}, right_held{};
  bool enter_down{}, enter_up{}, enter_held{};
  bool escape_down{}, escape_up{}, escape_held{};
  bool plus_down{};
  bool quicksave_down{}, quicksave_up{}, quicksave_held{};
};
// libnx retains at most 17 recent Npad samples. An extra slot is kept for
// the fallback/latest state when no new sample is available.
inline constexpr std::size_t kMaxInputSnapshots = 18;
struct RawHistoryEntry {
  std::uint64_t sequence{};
  RawInput raw{};
};
struct PollBatch {
  std::array<Snapshot, kMaxInputSnapshots> snapshots{};
  std::size_t count{};
  std::uint64_t missed_samples{};
  std::uint64_t newest_sequence{};
};
struct State {
  Config config{};
  Snapshot last{};
  RawInput host_raw{};
  double precise_x{};
  double precise_y{};
  std::uint64_t last_update_us{};
  bool initialized{};
  bool time_initialized{};
  bool plus_held{};
  std::uint64_t last_hid_sequence{};
  std::uint64_t hid_source_key{};
  bool hid_history_ready{};
  bool touch_was_active{};
};

Snapshot NormalizeAt(State* state, RawInput raw, std::int32_t width,
                     std::int32_t height, std::uint64_t now_us);
Snapshot Normalize(State* state, RawInput raw, std::int32_t width, std::int32_t height);
void InjectHostRaw(State* state, RawInput raw);
void SuppressUntilReleased();
void SetArcadeControls(bool enabled);
struct FlightStick { std::int32_t x{}, y{}; };
FlightStick GetFlightStick();
void Warp(State* state, std::int32_t x, std::int32_t y,
          std::int32_t width, std::int32_t height);
Snapshot Poll(State* state, std::int32_t width, std::int32_t height);
PollBatch NormalizeHistoryAt(State* state, const RawHistoryEntry* entries,
                             std::size_t count, RawInput fallback,
                             std::int32_t width, std::int32_t height,
                             std::uint64_t now_us);
PollBatch PollHistory(State* state, std::int32_t width, std::int32_t height);

}  // namespace srhd_awa::platform::input_platform

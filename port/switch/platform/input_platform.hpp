#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "controller_actions.hpp"

namespace srhd_awa::platform::input_platform {

struct Config {
  std::int32_t stick_deadzone{5500};
  double stick_pixels_per_second{600.0};
  double dpad_pixels_per_second{180.0};
  std::uint64_t max_delta_us{50000};  // Motion after an actual stall.
  std::uint64_t stall_threshold_us{200000};
  bool invert_x{}, invert_y{}, pointer_acceleration{};
};
enum class ControllerStyle : std::uint8_t { Host, Handheld, JoyDual, FullKey, Unsupported };
struct RawInput {
  std::int32_t stick_x{}, stick_y{};
  bool dpad_left{}, dpad_right{}, dpad_up{}, dpad_down{};
  bool a{}, b{}, x{}, y{}, plus{};
  bool zl{}, zr{}, minus{};
  bool l{}, r{};
  bool l3{}, r3{};
  bool touch{};
  std::int32_t touch_x{}, touch_y{}; // HID coordinates in the 1280x720 panel.
  std::int32_t right_stick_x{}, right_stick_y{};
  bool connected{true}; // Host injection defaults to one connected controller.
  ControllerStyle controller_style{ControllerStyle::Host};
};
struct Snapshot {
  controller_actions::Events actions{};
  ControllerStyle controller_style{ControllerStyle::Host};
  bool connected{true};
  bool touch_held{}, touch_message{}; // Provenance travels with the queued mouse event.
  std::uint16_t flight_keys{}; // movement/fire, main/secondary weapon cycle, autopilot, arcade only
  std::int32_t x{}, y{};
  std::uint64_t hid_sequence{};
  bool left_down{}, left_up{}, left_held{};
  bool right_down{}, right_up{}, right_held{};
  bool enter_down{}, enter_up{}, enter_held{};
  bool escape_down{}, escape_up{}, escape_held{};
  bool plus_down{};
  bool pause_down{}, pause_up{}, pause_held{};
  bool result_confirm_held{};
  bool ui_focus{}, ui_mode_changed{};
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
  controller_actions::Router actions;
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
  bool left_stick_blocked{}, right_stick_blocked{};
  std::uint64_t history_owner_token{};
  bool focus_initialized{}, ui_focus{}, last_l3{};
  bool ui_available{}, ui_preferred_focus{};
  std::uint64_t ui_owner_token{}, ui_profile_revision{}, previous_buttons{}, pointer_hold_us{};
  std::int32_t pointer_sign_x{}, pointer_sign_y{};
};

Snapshot NormalizeAt(State* state, RawInput raw, std::int32_t width,
                     std::int32_t height, std::uint64_t now_us);
Snapshot Normalize(State* state, RawInput raw, std::int32_t width, std::int32_t height);
void InjectHostRaw(State* state, RawInput raw);
void SuppressUntilReleased();
void SetArcadeControls(bool enabled);
void SetInputContext(controller_actions::Context context);
controller_actions::Context GetInputContext();
void SetUiFocusPolicy(bool available, bool prefer_focus);
bool UiFocusMode();
void SyncControllerSettings(State* state);
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

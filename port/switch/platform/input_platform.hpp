#pragma once

#include <cstdint>

namespace srhd_awa::platform::input_platform {

struct Config {
  std::int32_t stick_deadzone{5500};
  std::int32_t pointer_speed{12};
  std::int32_t dpad_step{3};
};
struct RawInput {
  std::int32_t stick_x{}, stick_y{};
  bool dpad_left{}, dpad_right{}, dpad_up{}, dpad_down{};
  bool a{}, b{}, x{}, y{}, plus{};
};
struct Snapshot {
  std::int32_t x{}, y{};
  bool left_down{}, left_up{}, left_held{};
  bool right_down{}, right_up{}, right_held{};
  bool enter_down{}, enter_up{}, enter_held{};
  bool escape_down{}, escape_up{}, escape_held{};
  bool plus_down{};
};
struct State {
  Config config{};
  Snapshot last{};
  RawInput host_raw{};
  bool initialized{};
  bool plus_held{};
};

Snapshot Normalize(State* state, RawInput raw, std::int32_t width, std::int32_t height);
void InjectHostRaw(State* state, RawInput raw);
void Warp(State* state, std::int32_t x, std::int32_t y,
          std::int32_t width, std::int32_t height);
Snapshot Poll(State* state, std::int32_t width, std::int32_t height);

}  // namespace srhd_awa::platform::input_platform

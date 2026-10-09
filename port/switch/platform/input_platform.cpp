#include "input_platform.hpp"

#include <algorithm>
#include <cstdint>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::input_platform {
namespace {
std::int32_t Axis(std::int32_t value, const Config& config) {
  value = std::clamp(value, -32767, 32767);
  if (value > -config.stick_deadzone && value < config.stick_deadzone) return 0;
  return static_cast<std::int32_t>(static_cast<std::int64_t>(value) * config.pointer_speed / 32767);
}
}  // namespace

Snapshot Normalize(State* state, RawInput raw, std::int32_t width, std::int32_t height) {
  if (!state || width <= 0 || height <= 0) return {};
  if (!state->initialized) {
    state->last.x = width / 2;
    state->last.y = height / 2;
    state->initialized = true;
  }
  const auto& config = state->config;
  Snapshot result{};
  result.x = static_cast<std::int32_t>(std::clamp<std::int64_t>(
      static_cast<std::int64_t>(state->last.x) + Axis(raw.stick_x, config) +
      (raw.dpad_right - raw.dpad_left) * config.dpad_step, 0, width - 1));
  result.y = static_cast<std::int32_t>(std::clamp<std::int64_t>(
      static_cast<std::int64_t>(state->last.y) - Axis(raw.stick_y, config) +
      (raw.dpad_down - raw.dpad_up) * config.dpad_step, 0, height - 1));
  result.left_down = raw.a && !state->last.left_held;
  result.left_up = !raw.a && state->last.left_held;
  result.left_held = raw.a;
  result.right_down = raw.b && !state->last.right_held;
  result.right_up = !raw.b && state->last.right_held;
  result.right_held = raw.b;
  result.plus_down = raw.plus && !state->plus_held;
  state->plus_held = raw.plus;
  state->last = result;
  return result;
}
void InjectHostRaw(State* state, RawInput raw) { if (state) state->host_raw = raw; }
Snapshot Poll(State* state, std::int32_t width, std::int32_t height) {
  if (!state) return {};
#if defined(__SWITCH__)
  static PadState pad;
  static bool pad_initialized{};
  if (!pad_initialized) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    pad_initialized = true;
  }
  padUpdate(&pad);
  const auto held = padGetButtons(&pad);
  const auto stick = padGetStickPos(&pad, 0);
  RawInput raw{};
  raw.stick_x = stick.x; raw.stick_y = stick.y;
  raw.dpad_left = (held & HidNpadButton_Left) != 0;
  raw.dpad_right = (held & HidNpadButton_Right) != 0;
  raw.dpad_up = (held & HidNpadButton_Up) != 0;
  raw.dpad_down = (held & HidNpadButton_Down) != 0;
  raw.a = (held & HidNpadButton_A) != 0;
  raw.b = (held & HidNpadButton_B) != 0;
  raw.plus = (held & HidNpadButton_Plus) != 0;
  return Normalize(state, raw, width, height);
#else
  return Normalize(state, state->host_raw, width, height);
#endif
}

}  // namespace srhd_awa::platform::input_platform

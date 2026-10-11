#include "input_platform.hpp"
#include "e2e_clock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::input_platform {
namespace {
double Axis(std::int32_t value, const Config& config) {
  value = std::clamp(value, -32767, 32767);
  const auto magnitude = std::abs(value);
  const auto deadzone = std::clamp(config.stick_deadzone, 0, 32766);
  if (magnitude <= deadzone) return 0.0;
  const auto normalized = static_cast<double>(magnitude - deadzone) /
                          static_cast<double>(32767 - deadzone);
  const auto curved = normalized * normalized;
  return value < 0 ? -curved : curved;
}
}  // namespace

Snapshot NormalizeAt(State* state, RawInput raw, std::int32_t width,
                     std::int32_t height, std::uint64_t now_us) {
  if (!state || width <= 0 || height <= 0) return {};
  if (!state->initialized) {
    state->last.x = width / 2;
    state->last.y = height / 2;
    state->precise_x = state->last.x;
    state->precise_y = state->last.y;
    state->initialized = true;
  }
  std::uint64_t delta_us{};
  if (state->time_initialized && now_us >= state->last_update_us) {
    const auto elapsed_us = now_us - state->last_update_us;
    // Low frame rates still represent real held input. Only a much longer
    // resource-load pause is capped to avoid a single large cursor jump.
    delta_us = elapsed_us > state->config.stall_threshold_us
                   ? std::min(elapsed_us, state->config.max_delta_us)
                   : elapsed_us;
  }
  state->last_update_us = now_us;
  state->time_initialized = true;
  const auto& config = state->config;
  const auto seconds = static_cast<double>(delta_us) / 1000000.0;
  const auto dx = (Axis(raw.stick_x, config) * config.stick_pixels_per_second +
                   (static_cast<int>(raw.dpad_right) - static_cast<int>(raw.dpad_left)) *
                       config.dpad_pixels_per_second) * seconds;
  const auto dy = (-Axis(raw.stick_y, config) * config.stick_pixels_per_second +
                   (static_cast<int>(raw.dpad_down) - static_cast<int>(raw.dpad_up)) *
                       config.dpad_pixels_per_second) * seconds;
  state->precise_x = std::clamp(state->precise_x + dx, 0.0,
                                static_cast<double>(width - 1));
  state->precise_y = std::clamp(state->precise_y + dy, 0.0,
                                static_cast<double>(height - 1));
  Snapshot result{};
  result.x = static_cast<std::int32_t>(std::lround(state->precise_x));
  result.y = static_cast<std::int32_t>(std::lround(state->precise_y));
  result.left_down = raw.a && !state->last.left_held;
  result.left_up = !raw.a && state->last.left_held;
  result.left_held = raw.a;
  result.right_down = raw.b && !state->last.right_held;
  result.right_up = !raw.b && state->last.right_held;
  result.right_held = raw.b;
  result.enter_down = raw.x && !state->last.enter_held;
  result.enter_up = !raw.x && state->last.enter_held;
  result.enter_held = raw.x;
  result.escape_down = raw.y && !state->last.escape_held;
  result.escape_up = !raw.y && state->last.escape_held;
  result.escape_held = raw.y;
  result.plus_down = raw.plus && !state->plus_held;
  state->plus_held = raw.plus;
  state->last = result;
  return result;
}
Snapshot Normalize(State* state, RawInput raw, std::int32_t width,
                   std::int32_t height) {
  return NormalizeAt(state, raw, width, height, e2e_clock::Microseconds());
}
void InjectHostRaw(State* state, RawInput raw) { if (state) state->host_raw = raw; }
void Warp(State* state, std::int32_t x, std::int32_t y,
          std::int32_t width, std::int32_t height) {
  if (!state || width <= 0 || height <= 0) return;
  state->last.x = std::clamp(x, 0, width - 1);
  state->last.y = std::clamp(y, 0, height - 1);
  state->precise_x = state->last.x;
  state->precise_y = state->last.y;
  state->initialized = true;
  state->time_initialized = false;
}
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
  raw.x = (held & HidNpadButton_X) != 0;
  raw.y = (held & HidNpadButton_Y) != 0;
  raw.plus = (held & HidNpadButton_Plus) != 0;
  return Normalize(state, raw, width, height);
#else
  return Normalize(state, state->host_raw, width, height);
#endif
}

}  // namespace srhd_awa::platform::input_platform

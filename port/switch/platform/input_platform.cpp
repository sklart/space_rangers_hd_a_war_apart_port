#include "input_platform.hpp"
#include "e2e_clock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <atomic>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::input_platform {
namespace {
std::atomic_bool suppress_applet_input{false};
// Context and generation are published together; changing owner invalidates
// held buttons even if two windows share the same context type.
std::atomic<std::uint64_t> context_token{};
std::atomic<std::uint64_t> flight_stick{};
RawInput Neutral(const RawInput& source) {
  RawInput result{};
  result.connected = source.connected;
  result.controller_style = source.controller_style;
  return result;
}
bool Suppressed(const RawInput& raw) {
  if (!suppress_applet_input.load()) return false;
  if (!raw.touch && !raw.a && !raw.b && !raw.x && !raw.y && !raw.plus &&
      !raw.zl && !raw.zr && !raw.minus && !raw.l && !raw.r && !raw.dpad_left && !raw.dpad_right &&
      !raw.dpad_up && !raw.dpad_down &&
      std::abs(static_cast<std::int64_t>(raw.stick_x)) <= 5500 &&
      std::abs(static_cast<std::int64_t>(raw.stick_y)) <= 5500 &&
      std::abs(static_cast<std::int64_t>(raw.right_stick_x)) <= 5500 &&
      std::abs(static_cast<std::int64_t>(raw.right_stick_y)) <= 5500)
    suppress_applet_input.store(false);
  return true; // Discard the release sample and old applet history as well.
}
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
#if defined(__SWITCH__)
void SetControllerSource(RawInput& raw, const PadState& pad) {
  const auto style = padGetStyleSet(&pad);
  raw.controller_style = padIsHandheld(&pad) ? ControllerStyle::Handheld :
      (style & HidNpadStyleTag_NpadFullKey) ? ControllerStyle::FullKey :
      (style & HidNpadStyleTag_NpadJoyDual) ? ControllerStyle::JoyDual : ControllerStyle::Unsupported;
  raw.connected = padIsConnected(&pad) && raw.controller_style != ControllerStyle::Unsupported;
}
void ReadTouch(RawInput* raw) {
  static bool initialized = false;
  if (!initialized) { hidInitializeTouchScreen(); initialized = true; }
  HidTouchScreenState sample{};
  if (hidGetTouchScreenStates(&sample, 1) && sample.count) {
    raw->touch = true;
    raw->touch_x = sample.touches[0].x;
    raw->touch_y = sample.touches[0].y;
  }
}
RawInput FromButtons(std::uint64_t held, HidAnalogStickState stick, HidAnalogStickState right) {
  RawInput raw{};
  raw.stick_x = stick.x; raw.stick_y = stick.y;
  raw.right_stick_x = right.x; raw.right_stick_y = right.y;
  raw.dpad_left = (held & HidNpadButton_Left) != 0;
  raw.dpad_right = (held & HidNpadButton_Right) != 0;
  raw.dpad_up = (held & HidNpadButton_Up) != 0;
  raw.dpad_down = (held & HidNpadButton_Down) != 0;
  raw.a = (held & HidNpadButton_A) != 0;
  raw.b = (held & HidNpadButton_B) != 0;
  raw.x = (held & HidNpadButton_X) != 0;
  raw.y = (held & HidNpadButton_Y) != 0;
  raw.plus = (held & HidNpadButton_Plus) != 0;
  raw.zl = (held & HidNpadButton_ZL) != 0;
  raw.zr = (held & HidNpadButton_ZR) != 0;
  raw.minus = (held & HidNpadButton_Minus) != 0;
  raw.l = (held & HidNpadButton_L) != 0;
  raw.r = (held & HidNpadButton_R) != 0;
  return raw;
}

std::size_t ReadNpadHistory(const PadState& pad,
                            std::array<RawHistoryEntry, 17>& output,
                            std::uint64_t* source_key) {
  *source_key = 0;
  HidNpadCommonState samples[17]{};
  std::size_t count = 0;
  HidNpadIdType id = HidNpadIdType_No1;
  std::uint32_t style = 0;
  if (padIsNpadActive(&pad, HidNpadIdType_No1)) {
    style = hidGetNpadStyleSet(id);
    if (style & HidNpadStyleTag_NpadFullKey)
      count = hidGetNpadStatesFullKey(id, samples, 17);
    else if (style & HidNpadStyleTag_NpadJoyDual)
      count = hidGetNpadStatesJoyDual(id, samples, 17);
    else if (style & HidNpadStyleTag_NpadJoyLeft)
      count = hidGetNpadStatesJoyLeft(id, samples, 17);
    else if (style & HidNpadStyleTag_NpadJoyRight)
      count = hidGetNpadStatesJoyRight(id, samples, 17);
  } else if (padIsHandheld(&pad)) {
    id = HidNpadIdType_Handheld;
    style = hidGetNpadStyleSet(id);
    count = hidGetNpadStatesHandheld(id, samples, 17);
  }
  if (!count) return 0;
  *source_key = (static_cast<std::uint64_t>(id) << 32) | style;
  // libnx returns newest first. Sort by sampling_number to remain correct if
  // a future libnx revision changes the output order.
  for (std::size_t i = 0; i < count; ++i)
    output[i] = {samples[i].sampling_number,
                 FromButtons(samples[i].buttons, samples[i].analog_stick_l, samples[i].analog_stick_r)};
  std::sort(output.begin(), output.begin() + count,
            [](const auto& left, const auto& right) {
              return left.sequence < right.sequence;
            });
  return count;
}
#endif
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
  using namespace controller_actions;
  const auto token = context_token.load();
  const auto context = static_cast<Context>(token & 255);
  const auto button = [](bool held, Button bit) -> Mask { return held ? Mask(bit) : Mask{}; };
  const Mask buttons = button(raw.a, A) | button(raw.b, B) | button(raw.x, X) |
      button(raw.y, Y) | button(raw.plus, Plus) | button(raw.minus, Minus) |
      button(raw.l, L) | button(raw.r, R) | button(raw.zl, ZL) | button(raw.zr, ZR) |
      button(raw.dpad_up, Up) | button(raw.dpad_down, Down) |
      button(raw.dpad_left, Left) | button(raw.dpad_right, Right) | button(raw.touch, Touch);
  const auto source_token = token * 256 + static_cast<unsigned>(raw.controller_style);
  const auto actions = state->actions.Update(buttons, context, source_token, now_us,
                                            raw.connected, true);
  const auto displaced = [&](std::int32_t x, std::int32_t y) {
    const auto deadzone = std::clamp(config.stick_deadzone, 0, 32766);
    return std::abs(static_cast<std::int64_t>(x)) > deadzone ||
           std::abs(static_cast<std::int64_t>(y)) > deadzone;
  };
  if (actions.changed || !raw.connected) {
    state->left_stick_blocked = displaced(raw.stick_x, raw.stick_y);
    state->right_stick_blocked = displaced(raw.right_stick_x, raw.right_stick_y);
    delta_us = 0;
  }
  if (!displaced(raw.stick_x, raw.stick_y)) state->left_stick_blocked = false;
  if (!displaced(raw.right_stick_x, raw.right_stick_y)) state->right_stick_blocked = false;
  if (state->left_stick_blocked) raw.stick_x = raw.stick_y = 0;
  if (state->right_stick_blocked) raw.right_stick_x = raw.right_stick_y = 0;
  const auto accepted = actions.accepted_buttons;
  raw.a = accepted & A; raw.b = accepted & B; raw.x = accepted & X; raw.y = accepted & Y;
  raw.plus = accepted & Plus; raw.minus = accepted & Minus;
  raw.l = accepted & L; raw.r = accepted & R; raw.zl = accepted & ZL; raw.zr = accepted & ZR;
  raw.dpad_up = accepted & Up; raw.dpad_down = accepted & Down;
  raw.dpad_left = accepted & Left; raw.dpad_right = accepted & Right;
  raw.touch = accepted & Touch;
  if (!raw.connected || context == Context::ControllerDisconnected ||
      context == Context::SwitchAppletActive || context == Context::SuspendResume) {
    raw.stick_x = raw.stick_y = raw.right_stick_x = raw.right_stick_y = 0;
  }
  std::uint16_t flight = 0;
  if (context == Context::ArcadeGameplay) {
    const auto sx = std::clamp(raw.stick_x, -32767, 32767);
    const auto sy = std::clamp(raw.stick_y, -32767, 32767);
    const auto deadzone = std::clamp(config.stick_deadzone, 0, 32766);
    flight_stick.store(static_cast<std::int64_t>(sx) * sx + static_cast<std::int64_t>(sy) * sy >
        static_cast<std::int64_t>(deadzone) * deadzone ?
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(sx)) |
          (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sy)) << 32) : 0);
    raw.stick_x = raw.right_stick_x;
    raw.stick_y = raw.right_stick_y;
    flight = raw.dpad_up | (raw.dpad_down << 1) | (raw.dpad_left << 2) |
        (raw.dpad_right << 3) | (raw.zr << 4) | (raw.zl << 5) |
        (raw.l << 6) | (raw.r << 7) | (raw.minus << 8);
    if (raw.zl && raw.zr && raw.minus) flight &= 15; // Preserve quicksave chord.
    raw.dpad_left = raw.dpad_right = raw.dpad_up = raw.dpad_down = false;
  }
  if (raw.touch) {
    state->precise_x = std::clamp<std::int32_t>(
        static_cast<std::int64_t>(raw.touch_x) * width / 1280, 0, width - 1);
    state->precise_y = std::clamp<std::int32_t>(
        static_cast<std::int64_t>(raw.touch_y) * height / 720, 0, height - 1);
    raw.stick_x = raw.stick_y = 0;
    raw.dpad_left = raw.dpad_right = raw.dpad_up = raw.dpad_down = false;
  }
  const bool result_confirm = context == Context::ArcadeResult && raw.a;
  raw.a = (context == Context::ArcadeResult ? false : raw.a) || raw.touch;
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
  result.actions = actions;
  result.result_confirm_held = result_confirm;
  result.controller_style = raw.controller_style;
  result.connected = raw.connected;
  result.touch_held = raw.touch;
  result.touch_message = raw.touch || (state->last.touch_held && !raw.a);
  result.flight_keys = flight;
  result.x = static_cast<std::int32_t>(std::lround(state->precise_x));
  result.y = static_cast<std::int32_t>(std::lround(state->precise_y));
  result.left_down = raw.a && !state->last.left_held;
  result.left_up = !raw.a && state->last.left_held;
  result.left_held = raw.a;
  result.right_down = raw.y && !state->last.right_held;
  result.right_up = !raw.y && state->last.right_held;
  result.right_held = raw.y;
  result.enter_down = raw.x && !state->last.enter_held;
  result.enter_up = !raw.x && state->last.enter_held;
  result.enter_held = raw.x;
  result.escape_down = raw.b && !state->last.escape_held;
  result.escape_up = !raw.b && state->last.escape_held;
  result.escape_held = raw.b;
  result.plus_down = raw.plus && !state->plus_held;
  result.pause_held = actions.Has(Action::OpenPause);
  result.pause_down = (actions.pressed & Bit(Action::OpenPause)) != 0;
  result.pause_up = (actions.released & Bit(Action::OpenPause)) != 0;
  result.quicksave_held = raw.zl && raw.zr && raw.minus;
  result.quicksave_down = result.quicksave_held && !state->last.quicksave_held;
  result.quicksave_up = !result.quicksave_held && state->last.quicksave_held;
  state->plus_held = raw.plus;
  state->last = result;
  return result;
}
Snapshot Normalize(State* state, RawInput raw, std::int32_t width,
                   std::int32_t height) {
  return NormalizeAt(state, raw, width, height, e2e_clock::Microseconds());
}
void InjectHostRaw(State* state, RawInput raw) { if (state) state->host_raw = raw; }
void SuppressUntilReleased() { suppress_applet_input.store(true); }
void SetInputContext(controller_actions::Context context) {
  auto previous = context_token.load();
  while (!context_token.compare_exchange_weak(previous,
      ((previous >> 8) + 1) * 256 + static_cast<unsigned>(context))) {}
  flight_stick.store(0);
}
controller_actions::Context GetInputContext() {
  return static_cast<controller_actions::Context>(context_token.load() & 255);
}
void SetArcadeControls(bool enabled) {
  SetInputContext(enabled ? controller_actions::Context::ArcadeGameplay :
                           controller_actions::Context::MainMenu);
}
FlightStick GetFlightStick() {
  const auto packed = flight_stick.load();
  return {static_cast<std::int32_t>(packed), static_cast<std::int32_t>(packed >> 32)};
}
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
  auto raw = FromButtons(padGetButtons(&pad), padGetStickPos(&pad, 0), padGetStickPos(&pad, 1));
  SetControllerSource(raw, pad);
  ReadTouch(&raw);
  if (Suppressed(raw)) raw = Neutral(raw);
  return Normalize(state, raw, width, height);
#else
  const auto raw = Suppressed(state->host_raw) ? Neutral(state->host_raw) : state->host_raw;
  return Normalize(state, raw, width, height);
#endif
}

PollBatch NormalizeHistoryAt(State* state, const RawHistoryEntry* entries,
                             std::size_t count, RawInput fallback,
                             std::int32_t width, std::int32_t height,
                             std::uint64_t now_us) {
  PollBatch result{};
  if (!state) return result;
  const auto owner = context_token.load() * 256 + static_cast<unsigned>(fallback.controller_style);
  if (state->history_owner_token != owner || state->last.connected != fallback.connected)
    state->hid_history_ready = false; // Never replay the previous owner's HID history.
  state->history_owner_token = owner;
  auto append = [&](RawInput raw, std::uint64_t sample_us,
                    std::uint64_t sequence) {
    auto& snapshot = result.snapshots[result.count++];
    snapshot = NormalizeAt(state, raw, width, height, sample_us);
    snapshot.hid_sequence = sequence;
  };
  if (!entries || !count) {
    append(fallback, now_us, state->last_hid_sequence);
    return result;
  }
  count = std::min(count, kMaxInputSnapshots - 1);
  const auto newest = entries[count - 1].sequence;
  result.newest_sequence = newest;
  if (!state->hid_history_ready || newest <= state->last_hid_sequence) {
    state->last_hid_sequence = newest;
    state->hid_history_ready = true;
    append(entries[count - 1].raw, now_us, newest);
    return result;
  }
  const auto previous_sequence = state->last_hid_sequence;
  const auto previous_us = state->last_update_us;
  const auto elapsed_us = now_us >= previous_us ? now_us - previous_us : 0;
  const bool stalled = elapsed_us > state->config.stall_threshold_us;
  bool first = true;
  for (std::size_t i = 0; i < count; ++i) {
    const auto& entry = entries[i];
    if (entry.sequence <= previous_sequence) continue;
    if (first && entry.sequence > previous_sequence + 1)
      result.missed_samples = entry.sequence - previous_sequence - 1;
    first = false;
    const auto fraction = static_cast<long double>(entry.sequence - previous_sequence) /
                          static_cast<long double>(newest - previous_sequence);
    const auto sample_us = stalled ? now_us : previous_us +
        static_cast<std::uint64_t>(static_cast<long double>(elapsed_us) * fraction);
    append(entry.raw, sample_us, entry.sequence);
  }
  state->last_hid_sequence = newest;
  if (!result.count) append(fallback, now_us, newest);
  return result;
}

PollBatch PollHistory(State* state, std::int32_t width, std::int32_t height) {
  if (!state) return {};
#if defined(__SWITCH__)
  static PadState pad;
  static bool initialized{};
  if (!initialized) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    initialized = true;
  }
  padUpdate(&pad);
  auto fallback = FromButtons(padGetButtons(&pad), padGetStickPos(&pad, 0), padGetStickPos(&pad, 1));
  SetControllerSource(fallback, pad);
  ReadTouch(&fallback);
  if (Suppressed(fallback)) {
    state->hid_history_ready = false;
    return NormalizeHistoryAt(state, nullptr, 0, Neutral(fallback), width, height, e2e_clock::Microseconds());
  }
  std::array<RawHistoryEntry, 17> entries{};
  std::uint64_t source_key{};
  const auto count = ReadNpadHistory(pad, entries, &source_key);
  for (std::size_t i = 0; i < count; ++i) {
    entries[i].raw.connected = fallback.connected;
    entries[i].raw.controller_style = fallback.controller_style;
  }
  if (source_key != state->hid_source_key) {
    state->hid_source_key = source_key;
    state->hid_history_ready = false;
  }
  // Touch is sampled now, rather than replayed into older Npad samples. The
  // merged latest sample preserves transitions when switching finger/button.
  if (fallback.touch || state->touch_was_active) {
    state->touch_was_active = fallback.touch;
    state->hid_history_ready = false;
    return NormalizeHistoryAt(state, nullptr, 0, fallback,
                              width, height, e2e_clock::Microseconds());
  }
  return NormalizeHistoryAt(state, entries.data(), count, fallback,
                            width, height, e2e_clock::Microseconds());
#else
  const auto raw = Suppressed(state->host_raw) ? Neutral(state->host_raw) : state->host_raw;
  return NormalizeHistoryAt(state, nullptr, 0, raw,
                            width, height, e2e_clock::Microseconds());
#endif
}

}  // namespace srhd_awa::platform::input_platform

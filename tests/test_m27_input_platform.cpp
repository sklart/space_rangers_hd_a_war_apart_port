#include "input_platform.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <initializer_list>

using namespace srhd_awa::platform::input_platform;

namespace {
Snapshot RunForOneSecond(int polls, RawInput raw) {
  State state;
  auto frame = NormalizeAt(&state, raw, 1280, 720, 0);
  for (int i = 1; i <= polls; ++i)
    frame = NormalizeAt(&state, raw, 1280, 720,
                        static_cast<std::uint64_t>(i) * 1000000u / polls);
  return frame;
}
}  // namespace

int main() {
  RawInput full_stick{};
  full_stick.stick_x = 32767;
  const auto sixty = RunForOneSecond(60, full_stick);
  const auto six_hundred = RunForOneSecond(600, full_stick);
  const auto six_thousand = RunForOneSecond(6000, full_stick);
  assert(std::abs(sixty.x - six_hundred.x) <= 1);
  assert(std::abs(sixty.x - six_thousand.x) <= 1);
  assert(sixty.x == 1240 && sixty.y == 360);
  for (const int polls : {5, 10, 15, 20, 30}) {
    const auto slow = RunForOneSecond(polls, full_stick);
    assert(std::abs(slow.x - sixty.x) <= 1);
  }

  RawInput dpad{};
  dpad.dpad_right = true;
  assert(RunForOneSecond(60, dpad).x == 820);
  assert(RunForOneSecond(600, dpad).x == 820);
  assert(RunForOneSecond(6000, dpad).x == 820);
  for (const int polls : {5, 10, 15, 20, 30})
    assert(RunForOneSecond(polls, dpad).x == 820);

  RawInput deadzone{};
  deadzone.stick_x = 5500;
  assert(RunForOneSecond(60, deadzone).x == 640);
  deadzone.stick_x = 12000;
  const auto precise = RunForOneSecond(600, deadzone);
  assert(precise.x > 640 && precise.x < 700);

  State slow_poll;
  NormalizeAt(&slow_poll, full_stick, 1280, 720, 0);
  auto frame = NormalizeAt(&slow_poll, full_stick, 1280, 720, 200000);
  assert(frame.x == 760);
  frame = NormalizeAt(&slow_poll, full_stick, 1280, 720, 700000);
  assert(frame.x == 790);  // A 500 ms UI stall is capped; ordinary 20/30/60 FPS is unchanged.

  for (const std::uint64_t stall_us : {1000000ULL, 2000000ULL, 3000000ULL}) {
    State stalled;
    NormalizeAt(&stalled, full_stick, 1280, 720, 0);
    frame = NormalizeAt(&stalled, full_stick, 1280, 720, stall_us);
    assert(frame.x == 670);  // A long stall must not jump across the screen.
  }

  State diagonal;
  RawInput down_right{};
  down_right.stick_x = 32767;
  down_right.stick_y = -32767;
  NormalizeAt(&diagonal, down_right, 1280, 720, 0);
  for (int i = 1; i <= 50; ++i)
    frame = NormalizeAt(&diagonal, down_right, 1280, 720, i * 10000);
  assert(frame.x == 940 && frame.y == 660);

  State buttons;
  RawInput raw{};
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(frame.x == 640 && frame.y == 360);
  raw.a = true;
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(frame.left_down && frame.left_held && frame.x == 640);
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(!frame.left_down && frame.left_held);
  raw.a = false;
  raw.b = true;
  raw.x = true;
  raw.y = true;
  raw.plus = true;
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(frame.left_up && frame.right_down && frame.enter_down &&
         frame.escape_down && frame.plus_down);
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(!frame.plus_down && !frame.right_down);
  raw = {};
  raw.zl = raw.zr = true;
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(!frame.quicksave_down && !frame.quicksave_held);
  raw.minus = true;
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(frame.quicksave_down && frame.quicksave_held);
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(!frame.quicksave_down && frame.quicksave_held);
  raw.minus = false;
  frame = NormalizeAt(&buttons, raw, 1280, 720, 0);
  assert(frame.quicksave_up && !frame.quicksave_held);
  Warp(&buttons, 1200, 700, 1280, 720);
  frame = NormalizeAt(&buttons, full_stick, 1280, 720, 2000000);
  assert(frame.x == 1200 && frame.y == 700);

  State moving_click;
  NormalizeAt(&moving_click, full_stick, 1280, 720, 0);
  full_stick.a = true;
  frame = NormalizeAt(&moving_click, full_stick, 1280, 720, 100000);
  assert(frame.left_down && frame.x == 700 && frame.y == 360);
  frame = NormalizeAt(&moving_click, full_stick, 1280, 720, 200000);
  assert(!frame.left_down && frame.left_held && frame.x == 760);
}

#include "input_platform.hpp"

#include <cassert>

using namespace srhd_awa::platform::input_platform;

int main() {
  State state;
  auto frame = Normalize(&state, {}, 1280, 720);
  assert(frame.x == 640 && frame.y == 360);
  RawInput raw{};
  raw.stick_x = 5499;
  frame = Normalize(&state, raw, 1280, 720);
  assert(frame.x == 640);
  raw.stick_x = 32767; raw.stick_y = -32767; raw.a = true;
  frame = Normalize(&state, raw, 1280, 720);
  assert(frame.x == 652 && frame.y == 372 && frame.left_down && frame.left_held);
  frame = Normalize(&state, raw, 1280, 720);
  assert(!frame.left_down && frame.left_held);
  raw.a = false; raw.plus = true; raw.dpad_left = true;
  frame = Normalize(&state, raw, 1280, 720);
  assert(frame.left_up && frame.plus_down && frame.x == 673);
  raw.stick_x = 0; raw.stick_y = 0;
  for (int i = 0; i < 1000; ++i) frame = Normalize(&state, raw, 1280, 720);
  assert(frame.x == 0 && !frame.plus_down);
  raw.dpad_left = false; raw.dpad_up = true;
  for (int i = 0; i < 1000; ++i) frame = Normalize(&state, raw, 1280, 720);
  assert(frame.y == 0);
}

#include "e2e511_arcade_commands.hpp"
#include "input_platform.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::platform;
int main() {
  const bool secondary[]{false, true, false, true, false};
  auto group = [&](int index) { return secondary[index]; };
  assert(NextArcadeWeapon(0, 5, false, group) == 2);
  assert(NextArcadeWeapon(4, 5, false, group) == 0);
  assert(NextArcadeWeapon(1, 5, true, group) == 3);
  assert(NextArcadeWeapon(3, 5, true, group) == 1);
  assert(NextArcadeWeapon(-1, 5, true, group) == 1);
  assert(NextArcadeWeapon(0, 0, false, group) == -1);
  assert(NextArcadeWeapon(0, 6, false, group) == -1);
  assert(NextArcadeWeapon(0, 1, true, group) == -1);
  assert(NextArcadeWeapon(0, 1, false, group) == 0);
  input_platform::State state;
  input_platform::RawInput raw{};
  input_platform::SetArcadeControls(true);
  raw.zl = raw.zr = raw.l = raw.r = raw.minus = true;
  auto sample = input_platform::NormalizeAt(&state, raw, 1280, 720, 0);
  assert(sample.quicksave_down && !sample.flight_keys); // Reserved chord.
  raw.minus = false;
  sample = input_platform::NormalizeAt(&state, raw, 1280, 720, 10000);
  assert(sample.flight_keys == 240 && sample.quicksave_up);
  raw = {}; raw.minus = true;
  sample = input_platform::NormalizeAt(&state, raw, 1280, 720, 20000);
  assert(sample.flight_keys == 256 && !sample.quicksave_down);
  input_platform::SetArcadeControls(false);
  sample = input_platform::NormalizeAt(&state, raw, 1280, 720, 30000);
  assert(!sample.flight_keys && !sample.escape_down && !sample.enter_down);
  std::puts("arcade command groups, wrap, empty inventory, trigger/chord and mode isolation PASS");
}

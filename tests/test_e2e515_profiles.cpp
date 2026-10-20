#include "controller_profiles.hpp"
#include "controller_focus.hpp"
#include "input_platform.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace srhd_awa::platform;
using namespace input_platform;
using controller_actions::Action;
using controller_actions::Bit;

int Position(int fps, int seconds, bool accelerate) {
  controller_profiles::Settings settings;
  settings.profile = controller_profiles::Profile::Cursor;
  settings.acceleration = accelerate;
  assert(controller_profiles::Set(settings));
  SetUiFocusPolicy(false, false);
  State state;
  SyncControllerSettings(&state);
  RawInput raw{}; raw.stick_x = 32767;
  NormalizeAt(&state, raw, 10000, 10000, 0);
  Snapshot sample;
  for (int frame = 1; frame <= fps * seconds; ++frame)
    sample = NormalizeAt(&state, raw, 10000, 10000, std::uint64_t(frame) * 1000000 / fps);
  return sample.x;
}
int main() {
  using namespace controller_focus;
  std::vector<Node> nodes{{1, 0, 0, 10, 10}, {2, 30, 0, 10, 10}, {3, 60, 0, 10, 10},
                          {4, 0, 30, 10, 10}, {5, 30, 30, 10, 10}, {6, 60, 30, 10, 10}};
  assert(Navigate(nodes, 1, Direction::Right) == 2);
  assert(Navigate(nodes, 2, Direction::Down) == 5);
  assert(Navigate(nodes, 3, Direction::Right) == 1);
  nodes[1].enabled = false;
  assert(Navigate(nodes, 1, Direction::Right) == 3);
  assert(Navigate(nodes, 2, Direction::Down) == 1); // Disabled current recovers safely.
  assert(!First({}));
  controller_profiles::Settings settings, parsed;
  settings.pointer_speed = 650.123456789;
  settings.invert_y = settings.swap_confirm_cancel = true;
  assert(controller_profiles::Parse(controller_profiles::Serialize(settings), parsed));
  assert(parsed.pointer_speed == settings.pointer_speed && parsed.invert_y && parsed.swap_confirm_cancel);
  const auto saved = controller_profiles::Serialize(parsed);
  for (const auto* invalid : {"2 0 5500 650 0 0 0 0", "1 2 5500 650 0 0 0 0", "1 0 5500 nan 0 0 0 0",
      "1 0 5500 650 0 0 0 2", "1 0 5500 650 0 0 0 0 trailing"}) {
    assert(!controller_profiles::Parse(invalid, parsed));
    assert(controller_profiles::Serialize(parsed) == saved);
  }
  controller_profiles::ResetDefaults();
  SetInputContext(controller_actions::Context::MainMenu);
  SetUiFocusPolicy(true, true);
  State hybrid;
  RawInput raw{};
  auto frame = NormalizeAt(&hybrid, raw, 1280, 720, 0);
  assert(frame.ui_focus);
  raw.stick_x = 32767;
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 100000);
  assert(frame.actions.pressed & Bit(Action::NavigateRight));
  assert(frame.x == 640 && frame.y == 360);
  raw = {}; raw.a = true;
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 200000);
  assert(frame.actions.pressed & Bit(Action::Confirm));
  assert(!frame.left_down);
  raw.right_stick_x = 32767;
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 300000);
  assert(!frame.ui_focus && frame.ui_mode_changed && !frame.left_down);
  raw.a = false;
  NormalizeAt(&hybrid, raw, 1280, 720, 400000);
  raw.a = true;
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 500000);
  assert(frame.left_down); // A fresh press in cursor mode is a real click.
  raw.right_stick_x = 0; raw.dpad_down = true;
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 600000);
  assert(frame.ui_focus && frame.ui_mode_changed && !(frame.actions.pressed & Bit(Action::Confirm)));
  raw = {}; raw.y = true;
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 700000);
  assert((frame.actions.pressed & Bit(Action::OpenContextAction)) && !frame.right_down && !frame.right_held);
  settings = {};
  settings.profile = controller_profiles::Profile::Cursor;
  assert(controller_profiles::Set(settings));
  // Polling during modal teardown may see the new profile before the parent's
  // next Refresh publishes its new preference. That later policy must win.
  SetUiFocusPolicy(true, true);
  raw = {}; NormalizeAt(&hybrid, raw, 1280, 720, 800000);
  SetUiFocusPolicy(true, false);
  frame = NormalizeAt(&hybrid, raw, 1280, 720, 900000);
  assert(!frame.ui_focus);
  State cursor;
  raw = {}; NormalizeAt(&cursor, raw, 1280, 720, 0);
  raw.stick_x = 32767;
  frame = NormalizeAt(&cursor, raw, 1280, 720, 100000);
  assert(!frame.ui_focus && frame.x == 700);
  raw = {}; raw.l3 = true;
  frame = NormalizeAt(&cursor, raw, 1280, 720, 200000);
  assert(frame.ui_focus && frame.ui_mode_changed);
  raw = {}; raw.r3 = true;
  frame = NormalizeAt(&cursor, raw, 1280, 720, 300000);
  assert(frame.actions.pressed & Bit(Action::OpenControllerSettings));
  for (int fps : {20, 30, 60}) {
    assert(Position(fps, 1, false) == 5600);
    assert(std::abs(Position(fps, 1, true) - 6050) <= 1);
    assert(std::abs(Position(fps, 3, true) - 8450) <= 1);
  }
  SetUiFocusPolicy(false, false);
  settings = {}; settings.invert_y = true; settings.swap_confirm_cancel = true;
  controller_profiles::Set(settings);
  State swapped;
  SyncControllerSettings(&swapped);
  raw = {}; NormalizeAt(&swapped, raw, 1280, 720, 0);
  raw.a = true; raw.stick_y = 32767;
  frame = NormalizeAt(&swapped, raw, 1280, 720, 100000);
  assert(frame.escape_down && !frame.left_down && frame.y == 420);
  std::puts("PASS profiles: true focus/cursor routing, grid/disabled/wrap, mode hold cancellation, R3, settings codec, acceleration parity 20/30/60 FPS");
}

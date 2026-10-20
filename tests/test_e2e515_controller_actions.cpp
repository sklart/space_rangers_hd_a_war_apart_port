#include "controller_actions.hpp"
#include "input_platform.hpp"
#include <cassert>
#include <cstdio>

using namespace srhd_awa::platform;
using namespace controller_actions;

int main() {
  Router router;
  router.Update(0, Context::MainMenu, 0, 0);
  auto e = router.Update(A, Context::MainMenu, 0, 1);
  assert(e.pressed == Bit(Action::Confirm));
  e = router.Update(A, Context::MainMenu, 0, 1000000);
  assert(e.held == Bit(Action::Confirm) && !e.pressed && !e.repeat);
  // The button that opens a child cannot confirm the child, including when
  // both owners use the same context. Returning cannot replay it either.
  e = router.Update(A, Context::ModalDialog, 1, 1000001);
  assert(e.released == Bit(Action::Confirm) && !e.held && e.changed);
  assert(!router.Update(A, Context::ModalDialog, 2, 1000002).held);
  router.Update(0, Context::ModalDialog, 2, 1000003);
  assert(router.Update(A, Context::ModalDialog, 2, 1000004).pressed == Bit(Action::Confirm));
  assert(!router.Update(A, Context::MainMenu, 3, 1000005).held);
  router.Update(0, Context::MainMenu, 3, 1000006);
  e = router.Update(B | Y | Plus, Context::MainMenu, 3, 1000007);
  assert(e.pressed == (Bit(Action::Cancel) | Bit(Action::OpenContextAction) | Bit(Action::OpenPause)));

  Router touch;
  assert(touch.Update(Touch, Context::Trade, 0, 0).pressed == Bit(Action::Confirm));
  assert(!touch.Update(Touch | A, Context::Trade, 0, 1).pressed);
  assert(!touch.Update(A, Context::Trade, 0, 2).released);
  assert(touch.Update(0, Context::Trade, 0, 3).released == Bit(Action::Confirm));

  Router repeat;
  assert(repeat.Update(Down, Context::SaveLoad, 0, 0).pressed == Bit(Action::NavigateDown));
  assert(!repeat.Update(Down, Context::SaveLoad, 0, 299999).repeat);
  assert(repeat.Update(Down, Context::SaveLoad, 0, 300000).repeat == Bit(Action::NavigateDown));
  assert(!repeat.Update(Down, Context::SaveLoad, 0, 300001).repeat);
  assert(repeat.Update(Down, Context::SaveLoad, 0, 2000000).repeat == Bit(Action::NavigateDown));
  assert(!repeat.Update(Down, Context::SaveLoad, 0, 2000001).repeat);

  Router drag;
  assert(drag.Update(ZR, Context::Equipment, 0, 0).Has(Action::StartDrag));
  e = drag.Update(ZR, Context::Equipment, 0, 1, false);
  assert(!e.held && (e.released & Bit(Action::EndDrag)));
  assert(!drag.Update(ZR, Context::Equipment, 0, 2, true).held);
  drag.Update(0, Context::Equipment, 0, 3);
  assert(drag.Update(ZR, Context::Equipment, 0, 4).Has(Action::StartDrag));

  // Every context boundary drops held actions, not just dialog transitions.
  unsigned contexts = 0;
  for (unsigned i = 0; i <= static_cast<unsigned>(Context::SuspendResume); ++i) {
    Router boundary;
    boundary.Update(A | ZR, Context::MainMenu, 0, 0);
    e = boundary.Update(A | ZR, static_cast<Context>(i), 1, 1);
    assert(!e.held && !e.pressed && e.released);
    ++contexts;
  }
  Router arcade;
  e = arcade.Update(Up | ZR | ZL | Minus | L | R, Context::ArcadeGameplay, 0, 0);
  assert(e.Has(Action::MoveForward) && e.Has(Action::PrimaryFire) && e.Has(Action::SecondaryFire));
  assert(e.Has(Action::CyclePrimaryWeapon) && e.Has(Action::CycleSecondaryWeapon));
  assert(e.Has(Action::ToggleAutopilot) && !e.Has(Action::QuickSave));
  e = arcade.Update(Up | ZR | ZL | Minus, Context::ArcadeGameplay, 0, 1, true, true);
  assert(e.Has(Action::QuickSave) && !e.Has(Action::PrimaryFire) && !e.Has(Action::SecondaryFire));

  // Test the adapter actually used by the message pump, not only the router.
  using namespace input_platform;
  SetInputContext(Context::MainMenu);
  State state;
  RawInput raw{};
  NormalizeAt(&state, raw, 1280, 720, 0);
  raw.a = true;
  assert(NormalizeAt(&state, raw, 1280, 720, 1).left_down);
  SetInputContext(Context::ModalDialog);
  auto s = NormalizeAt(&state, raw, 1280, 720, 2);
  assert(s.left_up && !s.left_down && !s.left_held);
  assert(!NormalizeAt(&state, raw, 1280, 720, 3).left_down);
  raw = {};
  NormalizeAt(&state, raw, 1280, 720, 4);
  raw.b = true;
  s = NormalizeAt(&state, raw, 1280, 720, 5);
  assert(s.escape_down && !s.right_down && s.actions.Has(Action::Cancel));
  raw.b = false; raw.y = true;
  s = NormalizeAt(&state, raw, 1280, 720, 6);
  assert(s.escape_up && s.right_down && s.actions.Has(Action::OpenContextAction));
  raw.connected = false;
  s = NormalizeAt(&state, raw, 1280, 720, 7);
  assert(s.right_up && !s.right_held);
  raw.connected = true;
  assert(!NormalizeAt(&state, raw, 1280, 720, 8).right_down);
  raw = {}; raw.controller_style = ControllerStyle::Handheld;
  NormalizeAt(&state, raw, 1280, 720, 10);
  raw.a = true; raw.stick_x = 32767;
  assert(NormalizeAt(&state, raw, 1280, 720, 100010).left_down);
  raw.controller_style = ControllerStyle::FullKey;
  s = NormalizeAt(&state, raw, 1280, 720, 200010);
  const auto old_x = s.x;
  assert(s.left_up && !s.left_held);
  s = NormalizeAt(&state, raw, 1280, 720, 300010);
  assert(!s.left_down && s.x == old_x); // Reconnect/style switch must require neutral.
  raw.a = false; raw.stick_x = 0;
  NormalizeAt(&state, raw, 1280, 720, 400010);
  raw.a = true; raw.stick_x = 32767;
  s = NormalizeAt(&state, raw, 1280, 720, 500010);
  assert(s.left_down && s.x > old_x);
  SetInputContext(Context::MainMenu);
  State history;
  RawHistoryEntry initial{1, {}};
  NormalizeHistoryAt(&history, &initial, 1, {}, 1280, 720, 0);
  RawInput held{}; held.a = true;
  RawHistoryEntry press{2, held};
  assert(NormalizeHistoryAt(&history, &press, 1, held, 1280, 720, 100).snapshots[0].left_down);
  SetInputContext(Context::ModalDialog);
  RawHistoryEntry old_history[]{{3, {}}, {4, held}};
  const auto batch = NormalizeHistoryAt(&history, old_history, 2, held, 1280, 720, 200);
  assert(batch.count == 1 && batch.snapshots[0].left_up && !batch.snapshots[0].left_down);
  std::printf("controller actions PASS contexts=%u touch/confirm/ownership/repeat/drag/reconnect/adapter\n", contexts);
}

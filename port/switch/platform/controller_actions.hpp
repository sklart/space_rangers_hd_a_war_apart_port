#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>

namespace srhd_awa::platform::controller_actions {

enum class Context : std::uint8_t {
  MainMenu, ModalDialog, Options, TextEntry, Help, GalaxyMap, StarSystem,
  ShipFlight, Planet, Hangar, Equipment, Trade, QuestDialog, SaveLoad,
  ArcadeGameplay, ArcadePause, ArcadeResult, MatrixRTS, MatrixDirectControl,
  MatrixPause, MatrixResult, ControllerDisconnected, SwitchAppletActive, SuspendResume
};
enum class Action : std::uint8_t {
  Confirm, Cancel, OpenPause, NavigateUp, NavigateDown, NavigateLeft, NavigateRight,
  MovePointer, Pan, Zoom, Scroll, OpenContextAction, Inspect, SelectMultiple,
  StartDrag, EndDrag, NextTab, PrevTab, CycleTarget, NextWeapon, PrevWeapon,
  PrimaryFire, SecondaryFire, Move, Aim, IssueCommand, OpenCommandWheel,
  OpenMap, OpenTextKeyboard, QuickSave, BackToGame, MoveForward, MoveBackward,
  TurnLeft, TurnRight, ToggleAutopilot, CyclePrimaryWeapon, CycleSecondaryWeapon,
  ToggleNavigationMode, OpenControllerSettings, Count
};
using Mask = std::uint64_t;
static_assert(static_cast<unsigned>(Action::Count) <= 64);
constexpr Mask Bit(Action action) { return Mask{1} << static_cast<unsigned>(action); }
constexpr const char* Name(Action action) {
  constexpr const char* names[]{"Confirm", "Cancel", "OpenPause", "NavigateUp", "NavigateDown",
      "NavigateLeft", "NavigateRight", "MovePointer", "Pan", "Zoom", "Scroll", "OpenContextAction",
      "Inspect", "SelectMultiple", "StartDrag", "EndDrag", "NextTab", "PrevTab", "CycleTarget",
      "NextWeapon", "PrevWeapon", "PrimaryFire", "SecondaryFire", "Move", "Aim", "IssueCommand",
      "OpenCommandWheel", "OpenMap", "OpenTextKeyboard", "QuickSave", "BackToGame", "MoveForward",
      "MoveBackward", "TurnLeft", "TurnRight", "ToggleAutopilot", "CyclePrimaryWeapon", "CycleSecondaryWeapon",
      "ToggleNavigationMode", "OpenControllerSettings"};
  static_assert(sizeof(names) / sizeof(*names) == static_cast<unsigned>(Action::Count));
  return action < Action::Count ? names[static_cast<unsigned>(action)] : "Invalid";
}
enum Button : Mask {
  A = 1u << 0, B = 1u << 1, X = 1u << 2, Y = 1u << 3,
  Plus = 1u << 4, Minus = 1u << 5, L = 1u << 6, R = 1u << 7,
  ZL = 1u << 8, ZR = 1u << 9, Up = 1u << 10, Down = 1u << 11,
  Left = 1u << 12, Right = 1u << 13, Touch = 1u << 14, L3 = 1u << 15, R3 = 1u << 16
};
struct Events {
  Mask pressed{}, held{}, released{}, repeat{};
  Mask accepted_buttons{};
  Context context{};
  bool changed{};
  bool Has(Action action) const { return (held & Bit(action)) != 0; }
};

// No libnx, Win32, engine pointers or process-exit action at this boundary.
// A new owner must change epoch, even when it uses the same context enum.
class Router {
public:
  void BlockUntilReleased(Mask buttons) { blocked_ |= buttons; }
  Events Update(Mask buttons, Context context, std::uint64_t epoch,
                std::uint64_t now_us, bool available = true,
                bool enable_quicksave = false, std::uint64_t input_revision = 0) {
    const bool changed = initialized_ &&
        (context != context_ || epoch != epoch_ || available != available_ || input_revision != input_revision_);
    const auto previous = held_;
    if (changed || !available) blocked_ |= buttons;
    blocked_ &= buttons;
    auto accepted = available ? buttons & ~blocked_ : 0;
    Mask next{};
    const bool inactive = context == Context::ControllerDisconnected ||
        context == Context::SwitchAppletActive || context == Context::SuspendResume;
    if (inactive) { blocked_ |= buttons; accepted = 0; }
    auto bind = [&](Mask physical, Action action) {
      if (accepted & physical) next |= Bit(action);
    };
    bind(A | Touch, Action::Confirm);
    bind(B, Action::Cancel);
    bind(Plus, Action::OpenPause);
    bind(Y, Action::OpenContextAction);
    bind(X, Action::Inspect);
    bind(L3, Action::ToggleNavigationMode);
    bind(R3, Action::OpenControllerSettings);
    if (context == Context::ArcadeGameplay) {
      bind(Up, Action::MoveForward); bind(Down, Action::MoveBackward);
      bind(Left, Action::TurnLeft); bind(Right, Action::TurnRight);
      bind(ZR, Action::PrimaryFire); bind(ZL, Action::SecondaryFire);
      bind(L, Action::CyclePrimaryWeapon); bind(R, Action::CycleSecondaryWeapon);
      bind(Minus, Action::ToggleAutopilot);
    } else {
      bind(Up, Action::NavigateUp); bind(Down, Action::NavigateDown);
      bind(Left, Action::NavigateLeft); bind(Right, Action::NavigateRight);
      bind(L, Action::PrevTab); bind(R, Action::NextTab);
      bind(ZR, Action::StartDrag);
    }
    constexpr Mask chord = ZL | ZR | Minus;
    if (enable_quicksave && (accepted & chord) == chord) {
      next &= ~(Bit(Action::PrimaryFire) | Bit(Action::SecondaryFire) |
                Bit(Action::ToggleAutopilot) | Bit(Action::StartDrag));
      next |= Bit(Action::QuickSave);
    }
    Events result{next & ~previous, next, previous & ~next, 0, accepted, context, changed};
    for (const auto action : {Action::NavigateUp, Action::NavigateDown,
                             Action::NavigateLeft, Action::NavigateRight}) {
      const auto bit = Bit(action);
      auto& due = repeat_at_[static_cast<unsigned>(action)];
      if (result.pressed & bit) due = now_us + 300000;
      else if ((next & bit) && now_us >= due) {
        result.repeat |= bit;
        due = now_us + 90000; // At most one repeat after a stalled frame.
      }
      if (!(next & bit)) due = 0;
    }
    if (result.released & Bit(Action::StartDrag)) result.released |= Bit(Action::EndDrag);
    held_ = next; context_ = context; epoch_ = epoch;
    available_ = available; initialized_ = true;
    input_revision_ = input_revision;
    return result;
  }
private:
  Mask held_{}, blocked_{};
  std::array<std::uint64_t, static_cast<unsigned>(Action::Count)> repeat_at_{};
  Context context_{Context::MainMenu};
  std::uint64_t epoch_{};
  std::uint64_t input_revision_{};
  bool initialized_{}, available_{true};
};
} // namespace srhd_awa::platform::controller_actions

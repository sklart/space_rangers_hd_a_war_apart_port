#pragma once

#include "win32_compat_messages.hpp"
#include "e2e_stage.hpp"
#include "units/GlobalsV.hpp"
#include "units/GI_MessageLoop.hpp"
#include "types/GI_GraphButton.hpp"
#include "types/GI_Panel.hpp"
#include "types/ab_MainForm.hpp"
#include "controller_ui.hpp"

namespace srhd_awa::platform::controller_screen_context {
// Original Run pops on its normal return only. An exception must remove the
// entry while its loop is still alive, otherwise later modals retain a dangling
// parent. Do not remove another owner's entries on a corrupt/unexpected stack.
class MessageLoopStackGuard {
public:
  explicit MessageLoopStackGuard(GI_MessageLoop::TMessageLoopGI* loop)
      : loop_(loop), depth_(GI_MessageLoop::MessageLoopStack ?
          pas::list_count(GI_MessageLoop::MessageLoopStack) : 0) {}
  ~MessageLoopStackGuard() {
    auto* stack = GI_MessageLoop::MessageLoopStack;
    if (!stack || pas::list_count(stack) <= depth_) return;
    if (pas::list_count(stack) == depth_ + 1 &&
        pas::list_get(stack, depth_) == loop_) {
      GI_MessageLoop::PopMessageLoop(loop_);
    } else {
      e2e_stage::Log("input loop stack unwind mismatch; unrelated owners preserved");
    }
  }
  MessageLoopStackGuard(const MessageLoopStackGuard&) = delete;
  MessageLoopStackGuard& operator=(const MessageLoopStackGuard&) = delete;
private:
  GI_MessageLoop::TMessageLoopGI* loop_;
  int depth_;
};

inline controller_actions::Context ForLoop(const GI_MessageLoop::TMessageLoopGI* loop) {
  using controller_actions::Context;
  const auto is = [&](GlobalsV::TGameScreenId id) {
    return loop && loop == GlobalsV::RegisteredScreens[id];
  };
  if (is(GlobalsV::screenMainMenu)) return Context::MainMenu;
  if (is(GlobalsV::screenSettings) || is(GlobalsV::screenNewGame)) return Context::Options;
  if (is(GlobalsV::screenSaveManager) || is(GlobalsV::screenGameLoad)) return Context::SaveLoad;
  if (is(GlobalsV::screenStarMap)) return Context::StarSystem;
  if (is(GlobalsV::screenGalaxy)) return Context::GalaxyMap;
  if (is(GlobalsV::screenHangar)) return Context::Hangar;
  if (is(GlobalsV::screenShip) || is(GlobalsV::screenEquipmentShop)) return Context::Equipment;
  if (is(GlobalsV::screenGoodsShop)) return Context::Trade;
  if (is(GlobalsV::screenPlanet) || is(GlobalsV::screenPlanetNO) || is(GlobalsV::screenGovernment))
    return Context::Planet;
  if (is(GlobalsV::screenTalk) || is(GlobalsV::screenPlanetQuest) || is(GlobalsV::screenRuinsTalk))
    return Context::QuestDialog;
  if (is(GlobalsV::screenIntroduction) || is(GlobalsV::screenInfo)) return Context::Help;
  if (is(GlobalsV::screenArcadeBattle)) {
    const auto* arcade = pas::class_cast_if<ab_MainForm::TfAB*>(
        const_cast<GI_MessageLoop::TMessageLoopGI*>(loop));
    if (arcade && ((arcade->VictoryPanel && arcade->VictoryPanel->Active) ||
                  (arcade->DefeatPanel && arcade->DefeatPanel->Active))) return Context::ArcadeResult;
    if (arcade && arcade->SimulationPaused) return Context::ArcadePause;
    return Context::ArcadeGameplay;
  }
  return Context::ModalDialog;
}
inline void Refresh(GI_MessageLoop::TMessageLoopGI* loop) {
  win32_compat::UpdateActiveInputContext(loop, ForLoop(loop));
  controller_ui::Refresh(loop);
}

inline void CancelMomentaryButtons(GI_MessageLoop::TObjectGI* object) {
  if (!object) return;
  if (auto* button = pas::class_cast_if<GI_GraphButton::TGraphButtonGI*>(object)) {
    if (button->Down && (button->Kind == GI_GraphButton::gbkNormal ||
                         button->Kind == GI_GraphButton::gbkDisable)) {
      button->Down = false;
      if (button->MessageLoop) button->UpdateStateVisuals();
    }
  }
  for (auto* child = object->FirstChild; child; child = child->NextSibling)
    CancelMomentaryButtons(child);
}

// Cancel without ProcessLeftButtonUp: that method may invoke UpCallback and
// ExecuteOnPressCode even when a new window never received a matching down.
inline void Release(void* owner, const win32_compat::ReleasedInput& held) noexcept {
  auto* loop = static_cast<GI_MessageLoop::TMessageLoopGI*>(owner);
  if (!loop) return;
  try {
    if (auto* arcade = pas::class_cast_if<ab_MainForm::TfAB*>(loop)) {
      arcade->ForwardKeyDown = arcade->ReverseKeyDown = arcade->BrakeKeyDown = false;
      arcade->TurnLeftKeyDown = arcade->TurnRightKeyDown = false;
      arcade->PrimaryFireKeyDown = arcade->SecondaryFireKeyDown = false;
    }
    if (loop->RootUiObject) {
      CancelMomentaryButtons(loop->RootUiObject);
      for (unsigned key = 3; key < held.keys.size(); ++key)
        if (held.keys[key]) loop->RootUiObject->BroadcastKeyUp(key);
    }
  } catch (...) {
    e2e_stage::Log("input owner cancellation failed; context still reset");
  }
}
} // namespace srhd_awa::platform::controller_screen_context

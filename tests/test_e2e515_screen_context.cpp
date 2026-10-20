#include "controller_screen_context.hpp"
#include "renderer_platform.hpp"
#include "units/ab_MainForm.hpp"
#include "units/GI_GraphButton.hpp"
#include <cassert>
#include <cstdio>

int main() {
  using namespace srhd_awa::platform;
  namespace routing = controller_screen_context;
  using controller_actions::Context;
  renderer_platform::RendererConfig config;
  config.game_width = config.game_height = 64;
  config.minimap_buffer_size = 8;
  assert(renderer_platform::InitializeSoftwareRenderer(config, nullptr));
  auto* arcade = pas::construct_call<ab_MainForm::TfAB>(ab_MainForm::TfAB_Create);
  auto* child = pas::construct_call<GI_MessageLoop::TMessageLoopGI>(GI_MessageLoop::TMessageLoopGI_Create);
  arcade->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  arcade->RootUiObject->MessageLoop = arcade;
  child->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  child->RootUiObject->MessageLoop = child;
  auto* momentary = pas::construct_call<GI_GraphButton::TGraphButtonGI>(
      GI_GraphButton::TGraphButtonGI_Create, arcade->RootUiObject);
  auto* toggle = pas::construct_call<GI_GraphButton::TGraphButtonGI>(
      GI_GraphButton::TGraphButtonGI_Create, arcade->RootUiObject);
  toggle->Kind = GI_GraphButton::gbkFix;
  GlobalsV::RegisteredScreens[GlobalsV::screenArcadeBattle] = arcade;
  assert(routing::ForLoop(arcade) == Context::ArcadeGameplay);
  assert(routing::ForLoop(child) == Context::ModalDialog);
  for (unsigned repeat = 0; repeat < 10; ++repeat) {
    win32_compat::InputContextScope parent_scope(routing::ForLoop(arcade), arcade, &routing::Release);
    arcade->ForwardKeyDown = arcade->ReverseKeyDown = arcade->PrimaryFireKeyDown = true;
    arcade->SecondaryFireKeyDown = arcade->TurnLeftKeyDown = arcade->TurnRightKeyDown = true;
    momentary->Down = toggle->Down = true;
    try {
      win32_compat::InputContextScope child_scope(routing::ForLoop(child), child, &routing::Release);
      assert(input_platform::GetInputContext() == Context::ModalDialog);
      assert(!arcade->ForwardKeyDown && !arcade->ReverseKeyDown && !arcade->TurnLeftKeyDown &&
             !arcade->TurnRightKeyDown && !arcade->PrimaryFireKeyDown && !arcade->SecondaryFireKeyDown);
      assert(!momentary->Down && toggle->Down); // Cancel press, preserve toggle selection.
      throw 1;
    } catch (int) {}
    assert(input_platform::GetInputContext() == Context::ArcadeGameplay);
  }
  GlobalsV::RegisteredScreens[GlobalsV::screenArcadeBattle] = nullptr;
  pas::free(child);
  pas::free(arcade);
  renderer_platform::ShutdownSoftwareRenderer();
  std::puts("PASS actual TfAB/TObjectGI/TGraphButtonGI cancellation, fixed toggle preserved, 10 nested owner unwinds; no battle executed");
}

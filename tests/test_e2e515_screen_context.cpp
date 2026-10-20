#include "controller_screen_context.hpp"
#include "renderer_platform.hpp"
#include "units/ab_MainForm.hpp"
#include "units/GI_GraphButton.hpp"
#include "units/GI_Panel.hpp"
#include "units/GI_Edit.hpp"
#include "controller_text_entry.hpp"
#include "e2e5_text_input.hpp"
#include <cassert>
#include <cstdio>

namespace {
struct Keyboard { bool cancel{}, fail{}; unsigned calls{}; };
bool TestKeyboard(const srhd_awa::platform::controller_text_input::Request& request,
                  std::u16string& value, void* user) {
  using namespace srhd_awa::platform;
  auto& keyboard = *static_cast<Keyboard*>(user);
  ++keyboard.calls;
  assert(input_platform::GetInputContext() == controller_actions::Context::SwitchAppletActive);
  assert(request.max_length == 13 && request.initial == u"Старое");
  value = u"Ёж 42";
  if (keyboard.fail) throw 7;
  return !keyboard.cancel;
}
}

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
  arcade->VictoryPanel = pas::construct_call<GI_Panel::TPanelGI>(GI_Panel::TPanelGI_Create, arcade->RootUiObject);
  arcade->DefeatPanel = pas::construct_call<GI_Panel::TPanelGI>(GI_Panel::TPanelGI_Create, arcade->RootUiObject);
  arcade->VictoryPanel->Active = arcade->DefeatPanel->Active = false;
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
    routing::MessageLoopStackGuard parent_stack(arcade);
    GI_MessageLoop::PushMessageLoop(arcade);
    try {
      routing::MessageLoopStackGuard child_stack(child);
      GI_MessageLoop::PushMessageLoop(child);
      throw 1;
    } catch (int) {}
    assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 1);
    assert(pas::list_get(GI_MessageLoop::MessageLoopStack, 0) == arcade);
    // Normal Pop remains authoritative; the guard must not pop twice.
    GI_MessageLoop::PopMessageLoop(arcade);
  }
  assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 0);
  {
    win32_compat::InputContextScope scope(Context::ArcadeGameplay, arcade, &routing::Release);
    arcade->PrimaryFireKeyDown = true;
    arcade->SimulationPaused = true;
    routing::Refresh(arcade);
    assert(input_platform::GetInputContext() == Context::ArcadePause && !arcade->PrimaryFireKeyDown);
    arcade->DefeatPanel->Active = true;
    routing::Refresh(arcade);
    assert(input_platform::GetInputContext() == Context::ArcadeResult);
    arcade->DefeatPanel->Active = false;
    arcade->SimulationPaused = false;
    routing::Refresh(arcade);
    assert(input_platform::GetInputContext() == Context::ArcadeGameplay);
    Keyboard keyboard;
    e2e5_text_input::SetHostBackend(&TestKeyboard, &keyboard);
    pas::WideString name(u"Старое");
    assert(e2e5_text_input::EditPlayerName(name) && name == u"Ёж 42");
    assert(input_platform::GetInputContext() == Context::ArcadeGameplay);
    name = pas::WideString(u"Старое");
    keyboard.cancel = true;
    assert(!e2e5_text_input::EditPlayerName(name) && name == u"Старое");
    assert(input_platform::GetInputContext() == Context::ArcadeGameplay);
    keyboard.cancel = false; keyboard.fail = true;
    try { e2e5_text_input::EditPlayerName(name); assert(false); }
    catch (int error) { assert(error == 7); }
    assert(name == u"Старое" && input_platform::GetInputContext() == Context::ArcadeGameplay);
    assert(keyboard.calls == 3);
    auto* field = pas::construct_call<GI_Edit::TEditGI>(GI_Edit::TEditGI_Create, arcade->RootUiObject);
    field->ControlName = pas::WideString(u"PlayerName");
    field->MaxLength = 13;
    field->SetText(pas::WideString(u"Старое"));
    field->SetCaretPosition(3);
    keyboard.cancel = true; keyboard.fail = false;
    assert(!controller_text_entry::Open(field));
    assert(field->Text == u"Старое" && field->CaretPosition == 3 && keyboard.calls == 4);
    e2e5_text_input::SetHostBackend(nullptr);
    input_platform::State input;
    input_platform::RawInput raw{}; raw.a = true;
    input_platform::InjectHostRaw(&input, raw);
    assert(!input_platform::PollHistory(&input, 64, 64).snapshots[0].left_down);
    input_platform::InjectHostRaw(&input, {});
    input_platform::PollHistory(&input, 64, 64);
    input_platform::InjectHostRaw(&input, raw);
    assert(input_platform::PollHistory(&input, 64, 64).snapshots[0].left_down);
  }
  for (unsigned repeat = 0; repeat < 10; ++repeat) {
    win32_compat::InputContextScope parent_scope(routing::ForLoop(arcade), arcade, &routing::Release);
    arcade->ForwardKeyDown = arcade->ReverseKeyDown = arcade->PrimaryFireKeyDown = true;
    arcade->SecondaryFireKeyDown = arcade->TurnLeftKeyDown = arcade->TurnRightKeyDown = true;
    momentary->Down = toggle->Down = true;
    try {
      win32_compat::InputContextScope child_scope(routing::ForLoop(child), child, &routing::Release);
      routing::Refresh(arcade); // A paused/background owner cannot steal a modal's input.
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
  pas::free(GI_MessageLoop::MessageLoopStack);
  GI_MessageLoop::MessageLoopStack = nullptr;
  renderer_platform::ShutdownSoftwareRenderer();
  std::puts("PASS real TfAB/TObjectGI/TGraphButtonGI/TEditGI cancellation, fixed toggle, 10 stack/owner unwinds, injected OSK Unicode/Cancel/exception; no battle or native applet executed");
}

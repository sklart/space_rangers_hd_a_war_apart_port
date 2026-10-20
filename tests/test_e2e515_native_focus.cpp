#include "controller_screen_context.hpp"
#include "renderer_platform.hpp"
#include "win32_compat_window.hpp"
#include "units/GI_GraphButton.hpp"
#include "units/fTalk.hpp"
#include "units/GI_Edit.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>

using namespace srhd_awa::platform;
namespace {
unsigned clicked[4]{};
unsigned context_clicked{};
unsigned choice_edge{};
unsigned slots_selected{};
void Click(GI_MessageLoop::TObjectGI* sender) { ++clicked[sender->UserIndex]; }
void ContextClick(GI_MessageLoop::TObjectGI*, std::uint32_t, WindowsSdk::TPoint) { ++context_clicked; }
void ChoicePress(GI_MessageLoop::TObjectGI*, std::uint32_t, WindowsSdk::TPoint) { assert(choice_edge == 0); choice_edge = 1; }
void ChoiceRelease(GI_MessageLoop::TObjectGI*, std::uint32_t, WindowsSdk::TPoint) { assert(choice_edge == 1); choice_edge = 2; }
void ChoiceResult(std::int32_t) {}
void SlotSelect(GI_MessageLoop::TObjectGI* sender, std::uint32_t, WindowsSdk::TPoint) {
  ++slots_selected;
  auto* owner = pas::class_cast_if<fSaveManager::TfSaveManager*>(sender->MessageLoop);
  assert(owner); owner->SelectedSlot = static_cast<std::int32_t>(sender->UserValue);
  pas::class_cast_if<GI_Edit::TEditGI*>(sender->FirstChild)->SetText(pas::WideString(u"original"));
}
GI_GraphButton::TGraphButtonGI* Button(GI_MessageLoop::TObjectGI* root, int index, int x, int y) {
  auto* button = pas::construct_call<GI_GraphButton::TGraphButtonGI>(GI_GraphButton::TGraphButtonGI_Create, root);
  button->UserIndex = index;
  button->SetPosition({x,y}); button->SetSize({40,20});
  button->HitKind = GI_GraphButton::gbhRect;
  button->UpCallback = pas::bind_static_method<&Click>(root->MessageLoop);
  return button;
}
}
int main() {
  namespace ui = controller_ui;
  using controller_actions::Context;
  renderer_platform::RendererConfig config;
  config.game_width = 256; config.game_height = 160; config.minimap_buffer_size = 8;
  assert(renderer_platform::InitializeSoftwareRenderer(config, nullptr));
  const auto fixture = std::filesystem::current_path() / "focus-user";
  user_root::SetRoot(fixture.string()); assert(user_root::EnsureLayout());
  controller_profiles::ResetDefaults();
  auto* loop = pas::construct_call<GI_MessageLoop::TMessageLoopGI>(GI_MessageLoop::TMessageLoopGI_Create);
  loop->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  loop->RootUiObject->MessageLoop = loop; loop->ContentPanel = loop->RootUiObject;
  loop->RootUiObject->LeftButtonDownCallback = pas::bind_static_method<&ContextClick>(loop);
  loop->ViewportRect = {0,0,256,160}; loop->RootUiObject->SetSize({256,160});
  auto* first = Button(loop->RootUiObject, 0, 10, 10); first->ControlName = pas::WideString(u"New");
  first->RightButtonDownCallback = pas::bind_static_method<&ContextClick>(loop);
  auto* second = Button(loop->RootUiObject, 1, 10, 50);
  auto* disabled = Button(loop->RootUiObject, 2, 80, 10);
  disabled->Kind = GI_GraphButton::gbkDisable; disabled->Disabled = true;
  auto* hidden = Button(loop->RootUiObject, 3, 80, 50); hidden->Active = false;
  loop->RootUiObject->UpdateAbsolutePosition(); loop->RootUiObject->UpdateSubtreeHitBounds();
  const auto window = win32_compat::RegisterMainWindow(nullptr, 256, 160);
  using Peek = std::int32_t (*)(Windows::TMsg*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
  const auto peek = reinterpret_cast<Peek>(win32_compat::ResolveMessageImport("user32.dll", "PeekMessageW"));
  Windows::TMsg message{};
  auto drain = [&] {
    unsigned delivered = 0;
    while (peek(&message, 0, controller_messages::kMessage, controller_messages::kMessage, 1)) {
      loop->ProcessWindowMessage(message.message, message.wParam, message.lParam);
      assert(++delivered < 100);
    }
    return delivered;
  };
  {
    win32_compat::InputContextScope input(Context::MainMenu, loop, &controller_screen_context::Release);
    ui::Scope scope(loop); ui::Refresh(loop);
    assert(scope.entries.size() == 2 && scope.selected == reinterpret_cast<std::uintptr_t>(first));
    const auto initial_sample = win32_compat::InjectHostInputTracked({});
    assert(!win32_compat::HostInputConsumed(initial_sample));
    drain();
    assert(win32_compat::HostInputConsumed(initial_sample));
    GR_Main::ScreenRenderBuffer->FillPixels16(0);
    loop->FinishQueuedDraw();
    const auto* row = reinterpret_cast<const std::uint16_t*>(
        static_cast<const unsigned char*>(GR_Main::ScreenRenderBuffer->GetPixels()) + 8 * GR_Main::ScreenRenderBuffer->PitchBytes);
    assert(row[8] == 0xffe0); // Actual RGB565 focus frame, through effective FinishQueuedDraw.
    input_platform::RawInput raw{}; raw.a = true;
    win32_compat::InjectHostInput(raw); assert(drain() == 1);
    assert(clicked[0] == 1 && !peek(&message,0,0x201,0x201,1));
    assert(drain() == 0 && clicked[0] == 1); // Held A does not confirm again.
    raw = {}; win32_compat::InjectHostInput(raw); drain();
    raw.y = true; win32_compat::InjectHostInput(raw); drain();
    assert(context_clicked == 1 && !peek(&message,0,0x204,0x204,1));
    assert(drain() == 0 && context_clicked == 1);
    raw = {}; win32_compat::InjectHostInput(raw); drain();
    raw.dpad_down = true; win32_compat::InjectHostInput(raw); drain();
    assert(scope.selected == reinterpret_cast<std::uintptr_t>(second));
    raw = {}; win32_compat::InjectHostInput(raw); drain();
    raw.a = true; win32_compat::InjectHostInput(raw); drain();
    assert(clicked[1] == 1 && !clicked[2] && !clicked[3]);
    raw = {}; win32_compat::InjectHostInput(raw); drain();
    first->Down = true;
    raw.right_stick_x = 32767; win32_compat::InjectHostInput(raw); drain();
    assert(!scope.focus && !first->Down && clicked[0] == 1);
    const auto selected = scope.selected;
    {
      win32_compat::InputContextScope modal(Context::ModalDialog, nullptr, nullptr);
      ui::Scope child(nullptr);
    }
    assert(ui::active == &scope && scope.selected == selected);
    controller_profiles::Settings cursor;
    cursor.profile = controller_profiles::Profile::Cursor;
    assert(controller_profiles::Set(cursor)); ui::Refresh(loop);
    auto* dialog = pas::construct_call<GI_MessageBox::TMessageBoxGI>(GI_MessageLoop::TMessageLoopGI_Create);
    dialog->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
    dialog->RootUiObject->MessageLoop = dialog; dialog->ContentPanel = dialog->RootUiObject;
    dialog->ViewportRect = {0,0,256,160}; dialog->RootUiObject->SetSize({256,160});
    Button(dialog->RootUiObject, 0, 10, 10);
    dialog->RootUiObject->UpdateAbsolutePosition(); dialog->RootUiObject->UpdateSubtreeHitBounds();
    {
      win32_compat::InputContextScope modal(Context::ModalDialog, dialog, nullptr);
      ui::Scope child(dialog); ui::preferences.active = true;
      ui::Refresh(dialog);
      assert(child.focus && child.entries.size() == 1); // Cursor settings still support X/reset without a pointer.
      ui::preferences.active = false;
    }
    pas::free(dialog);
    win32_compat::InjectHostInput({}); drain();
    raw = {}; raw.a = true; win32_compat::InjectHostInput(raw); drain();
    assert(peek(&message,0,0x201,0x201,1)); // Cursor profile uses the original pointer adapter.
    assert(clicked[0] == 1 && clicked[1] == 1);
    win32_compat::InjectHostInput({}); drain();
    assert(ui::SaveSettings(cursor));
    controller_profiles::Settings hybrid; hybrid.pointer_speed = 650;
    assert(ui::SaveSettings(hybrid)); // Existing target: copy/verify with backup, not rename-overwrite.
    std::ifstream saved(user_root::ConfigDirectory() + "/controller.ini");
    const std::string bytes{std::istreambuf_iterator<char>(saved), {}};
    controller_profiles::Settings parsed;
    assert(controller_profiles::Parse(bytes, parsed) && parsed.pointer_speed == 650);
    std::filesystem::create_directory(user_root::ConfigDirectory() + "/controller.ini.tmp");
    assert(!ui::SaveSettings(cursor));
    std::ifstream unchanged(user_root::ConfigDirectory() + "/controller.ini");
    assert(std::string(std::istreambuf_iterator<char>(unchanged), {}) == bytes);
  }
  win32_compat::UnregisterMainWindow(window);
  pas::free(loop);
  auto* talk = pas::construct_call<fTalk::TfTalk>(GI_MessageLoop::TMessageLoopGI_Create);
  talk->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  talk->RootUiObject->MessageLoop = talk;
  auto* choices = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, talk->RootUiObject);
  choices->ControlName = pas::WideString(u"TalkPA");
  auto* row = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, choices);
  row->MessageLoop = talk; row->HitTestBounds = {10,10,100,30};
  row->LeftButtonDownCallback = pas::bind_static_method<&ChoicePress>(talk);
  row->LeftButtonUpCallback = pas::bind_static_method<&ChoiceRelease>(talk);
  auto* choice = pas::construct_call<fTalk::TfTalkA>(fTalk::TfTalkA_Create);
  row->UserValue = e2e5_pointer_payload::Register(talk, choice);
  assert(!ui::Callable(row)); // Greyed choice with neither original transaction is not selectable.
  choice->Callback = pas::bind_static_method<&ChoiceResult>(talk);
  assert(ui::Callable(row)); ui::Activate(row); assert(choice_edge == 2);
  choices->ControlName = pas::WideString(u"OtherPanel");
  assert(!ui::Callable(row)); // Dual-edge controls outside audited dialog rows retain cursor fallback.
  e2e5_pointer_payload::ReleaseOwner(talk);
  pas::free(choice); pas::free(talk);
  auto* save = pas::construct_call<fSaveManager::TfSaveManager>(GI_MessageLoop::TMessageLoopGI_Create);
  save->SelectedSlot = -1;
  save->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  save->RootUiObject->MessageLoop = save;
  auto* panel = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, save->RootUiObject);
  panel->ControlName = pas::WideString(u"PanelSlot");
  auto* slot = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, panel);
  slot->MessageLoop = save; slot->UserValue = 5;
  slot->LeftButtonDownCallback = pas::bind_static_method<&SlotSelect>(save);
  auto* edit = pas::construct_call<GI_Edit::TEditGI>(GI_Edit::TEditGI_Create, slot);
  edit->MessageLoop = save; edit->MaxLength = 55;
  ui::Activate(edit);
  assert(slots_selected == 1 && save->SelectedSlot == 5 && save->FocusedControl == edit);
  edit->SetText(pas::WideString(u"Draft Ёж"));
  ui::Activate(edit);
  assert(slots_selected == 1 && edit->Text == u"Draft Ёж"); // Reopening OSK must not refresh away an unsaved name.
  pas::free(save);
  auto* goods = pas::construct_call<fGoodsShop2::TfGoodsShop2>(GI_MessageLoop::TMessageLoopGI_Create);
  goods->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  goods->RootUiObject->MessageLoop = goods;
  auto* trade_row = Button(goods->RootUiObject, 0, 10, 10);
  trade_row->MessageLoop = goods;
  trade_row->UpCallback = {};
  trade_row->ControlName = pas::WideString(u"Tov0");
  assert(ui::Callable(trade_row) && ui::GoodsRow(trade_row) == goods);
  trade_row->Disabled = true;
  assert(!ui::Callable(trade_row));
  trade_row->Disabled = false;
  trade_row->ControlName = pas::WideString(u"Tov8");
  assert(!ui::Callable(trade_row));
  trade_row->ControlName = pas::WideString(u"Tov10");
  assert(ui::Callable(trade_row));
  trade_row->UpCallback = pas::bind_static_method<&Click>(goods);
  const auto previous_clicks = clicked[0];
  assert(!ui::GoodsRow(trade_row));
  ui::Activate(trade_row);
  assert(clicked[0] == previous_clicks + 1); // Preserve a fork/custom callback.
  pas::free(goods);
  auto* quantity = pas::construct_call<fCount2::TfCount2>(GI_MessageLoop::TMessageLoopGI_Create);
  quantity->RootUiObject = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, nullptr);
  quantity->RootUiObject->MessageLoop = quantity; quantity->ContentPanel = quantity->RootUiObject;
  quantity->ViewportRect = {0,0,256,160}; quantity->RootUiObject->SetSize({256,160});
  auto* close = Button(quantity->RootUiObject, 0, 10, 100); close->ControlName = pas::WideString(u"Close");
  auto* accept = Button(quantity->RootUiObject, 1, 80, 100); accept->ControlName = pas::WideString(u"Ok");
  auto* slider = pas::construct_call<GI_MessageLoop::TObjectGI>(GI_MessageLoop::TObjectGI_Create, quantity->RootUiObject);
  slider->MessageLoop = quantity; slider->ControlName = pas::WideString(u"PanelBar");
  slider->LeftButtonDownCallback = pas::bind_static_method<&ContextClick>(quantity);
  slider->SetPosition({10,50}); slider->SetSize({100,20});
  assert(!ui::Callable(slider));
  quantity->RootUiObject->UpdateAbsolutePosition(); quantity->RootUiObject->UpdateSubtreeHitBounds();
  {
    ui::Scope scope(quantity); ui::Refresh(quantity);
    assert(ui::active->selected == reinterpret_cast<std::uintptr_t>(accept));
  }
  pas::free(quantity);
  renderer_platform::ShutdownSoftwareRenderer();
  std::puts("PASS real native buttons via injected pad, focus frame, disabled/hidden exclusion, no held/replay click, cursor fallback, focus restoration and settings write refusal");
}

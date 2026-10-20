#pragma once
#include "controller_focus.hpp"
#include "controller_profiles.hpp"
#include "controller_messages.hpp"
#include "e2e_stage.hpp"
#include "controller_text_entry.hpp"
#include "user_root.hpp"
#include "units/GI_MessageBox.hpp"
#include "units/GI_GraphButton.hpp"
#include "units/GI_Label.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "types/fTalk.hpp"
#include "types/fRuinsTalk.hpp"
#include "types/fSaveManager.hpp"
#include "types/fGoodsShop2.hpp"
#include "types/fCount2.hpp"
#if defined(E2E5_HOST_SDL_WINDOW)
#include "types/BreakMessageGIException.hpp"
namespace srhd_awa::platform::e2e515_save_checkpoint { void ObserveCurrentWorld(const char* phase); }
#endif
#include "e2e5_pointer_payload.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace srhd_awa::platform::controller_ui {
using controller_actions::Action;
using controller_actions::Context;
using controller_messages::Edge;
struct Entry { controller_focus::Node node; GI_MessageLoop::TObjectGI* control; };
class Scope;
inline thread_local Scope* active{};
inline bool PreferencesFocus(GI_MessageLoop::TMessageLoopGI* loop);
inline thread_local std::string loaded_path;
inline thread_local std::uint64_t scope_serial{};
inline bool ReadConfig(const std::string& path, std::string& text) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char bytes[1025]{};
  input.read(bytes, sizeof(bytes));
  if (input.gcount() > 1024 || input.bad()) return false;
  text.assign(bytes, static_cast<std::size_t>(input.gcount()));
  return true;
}
inline void LoadSettings() {
  const auto path = user_root::ConfigDirectory() + "/controller.ini";
  if (loaded_path == path) return;
  loaded_path = path;
  for (const auto& candidate : {path, path + ".bak"}) {
    std::string text;
    controller_profiles::Settings value;
    if (ReadConfig(candidate, text) && controller_profiles::Parse(text, value)) {
      controller_profiles::Set(value); return;
    }
  }
}
inline bool SaveSettings(const controller_profiles::Settings& value) {
  const auto path = user_root::ConfigDirectory() + "/controller.ini";
  const auto temporary = path + ".tmp";
  const auto text = controller_profiles::Serialize(value);
  { std::ofstream file(temporary, std::ios::trunc); file << text; file.flush(); if (!file) return false; }
  { std::ifstream check(temporary); const std::string bytes{std::istreambuf_iterator<char>(check), {}};
    if (bytes != text) return false; }
  std::error_code error;
  std::string previous;
  const bool existed = std::filesystem::exists(path, error);
  if (error) return false;
  if (existed) {
    if (!ReadConfig(path, previous)) return false;
    std::ofstream backup(path + ".bak", std::ios::trunc); backup << previous; backup.flush();
    if (!backup) return false;
  }
  std::filesystem::copy_file(temporary, path, std::filesystem::copy_options::overwrite_existing, error);
  std::ifstream check(path);
  const std::string bytes{std::istreambuf_iterator<char>(check), {}};
  if (!error && bytes == text) { std::filesystem::remove(temporary, error); return true; }
  if (existed) { std::ofstream restore(path, std::ios::trunc); restore << previous; restore.flush(); }
  return false; // Verified previous bytes also remain in .bak on rollback failure.
}

inline bool AllowedContext(Context context) {
  return context != Context::ArcadeGameplay && context != Context::ArcadePause &&
      context != Context::ArcadeResult && context != Context::MatrixRTS &&
      context != Context::MatrixDirectControl && context != Context::SwitchAppletActive &&
      context != Context::SuspendResume && context != Context::ControllerDisconnected;
}
inline fTalk::TfTalkA* DialogChoice(GI_MessageLoop::TObjectGI* object) {
  if (!object || !object->Parent || object->Parent->ControlName != u"TalkPA" ||
      !pas::assigned(object->LeftButtonDownCallback) || !pas::assigned(object->LeftButtonUpCallback) ||
      (!pas::class_cast_if<fTalk::TfTalk*>(object->MessageLoop) &&
       !pas::class_cast_if<fRuinsTalk::TfRuinsTalk*>(object->MessageLoop))) return nullptr;
  // Effective fTalk/fRuinsTalk store owner-scoped int32 tokens, not pointers.
  // Resolve through the same registry as their original choice-up handlers.
  if (object->UserValue >= 0 || object->UserValue < INT32_MIN) return nullptr;
  try {
    return e2e5_pointer_payload::Resolve<fTalk::TfTalkA*>(object->MessageLoop,
                                                       static_cast<std::int32_t>(object->UserValue));
  } catch (const std::runtime_error&) {
    return nullptr;
  }
}
inline fGoodsShop2::TfGoodsShop2* GoodsRow(GI_MessageLoop::TObjectGI* object) {
  if (!object) return nullptr;
  auto* shop = pas::class_cast_if<fGoodsShop2::TfGoodsShop2*>(object->MessageLoop);
  auto* button = pas::class_cast_if<GI_GraphButton::TGraphButtonGI*>(object);
  if (!shop || !button || button->Disabled || pas::assigned(button->DownCallback) ||
      pas::assigned(button->UpCallback) || button->OnPressCode) return nullptr;
  const auto name = static_cast<std::u16string_view>(object->ControlName);
  if (name.size() < 4 || name.substr(0, 3) != u"Tov") return nullptr;
  const bool market = name.size() == 4 && name[3] >= u'0' && name[3] <= u'7';
  const bool cargo = name.size() == 5 && name[3] == u'1' && name[4] >= u'0' && name[4] <= u'7';
  return market || cargo ? shop : nullptr;
}
inline bool Callable(GI_MessageLoop::TObjectGI* object) {
  if (const auto* choice = DialogChoice(object))
    return pas::assigned(choice->Callback) || pas::assigned(choice->FallbackCallback);
  if (pas::class_cast_if<GI_Edit::TEditGI*>(object)) return true;
  // This slider needs move/up delivery; a one-shot native Confirm cannot own
  // its drag. Keep it in the cursor adapter instead of starting a stuck drag.
  if (object->ControlName == u"PanelBar" && pas::class_cast_if<fCount2::TfCount2*>(object->MessageLoop)) return false;
  if (GoodsRow(object)) return true;
  if (auto* button = pas::class_cast_if<GI_GraphButton::TGraphButtonGI*>(object)) {
    if (button->Disabled && (button->Kind == GI_GraphButton::gbkDisable || button->Kind == GI_GraphButton::gbkFixDisable)) return false;
    // Dual-edge hold controls retain the cursor fallback until their complete
    // hold/release contract is mapped. Single-action buttons are direct calls.
    if ((button->Kind == GI_GraphButton::gbkNormal || button->Kind == GI_GraphButton::gbkDisable) &&
        pas::assigned(button->DownCallback) && pas::assigned(button->UpCallback)) return false;
    return pas::assigned(button->UpCallback) || pas::assigned(button->DownCallback) || button->OnPressCode;
  }
  return pas::assigned(object->LeftButtonDownCallback) && !pas::assigned(object->LeftButtonUpCallback);
}
inline void Collect(GI_MessageLoop::TObjectGI* object, GI_MessageLoop::TMessageLoopGI* loop,
                    std::vector<Entry>& entries) {
  if (!object || !object->Active || object->HitTestDisabled) return;
  const auto box = object->HitTestBounds;
  const auto width = std::int64_t(box.Right) - box.Left, height = std::int64_t(box.Bottom) - box.Top;
  const WindowsSdk::TPoint center{static_cast<std::int32_t>(std::int64_t(box.Left) + width / 2),
                                static_cast<std::int32_t>(std::int64_t(box.Top) + height / 2)};
  if (object != loop->ContentPanel && object != loop->RootUiObject &&
      width > 0 && height > 0 && width <= INT32_MAX && height <= INT32_MAX && Callable(object) &&
      center.X >= loop->ViewportRect.Left && center.X < loop->ViewportRect.Right &&
      center.Y >= loop->ViewportRect.Top && center.Y < loop->ViewportRect.Bottom &&
      !object->IsOccludedAtPoint(center))
    entries.push_back({{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object)),
                       box.Left, box.Top, box.Right - box.Left, box.Bottom - box.Top, true}, object});
  for (auto* child = object->FirstChild; child; child = child->NextSibling) Collect(child, loop, entries);
}

class Scope {
public:
  explicit Scope(GI_MessageLoop::TMessageLoopGI* owner) : owner(owner), previous(active) {
    LoadSettings(); active = this;
    focus = controller_profiles::Get().profile == controller_profiles::Profile::Hybrid;
    input_platform::SetUiFocusPolicy(false, focus);
  }
  ~Scope() {
    active = previous;
    if (previous) previous->trace_snapshot.clear();
    input_platform::SetUiFocusPolicy(previous && !previous->entries.empty(), previous && previous->focus);
  }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  GI_MessageLoop::TMessageLoopGI* owner;
  Scope* previous;
  std::vector<Entry> entries;
  std::vector<controller_focus::Node> graph;
  std::uint64_t selected{}, profile_revision{};
  const std::uint64_t serial{++scope_serial};
  std::string trace_snapshot;
  bool focus{};
};

// Opt-in host instrumentation publishes immutable snapshots on the UI thread.
// The pad driver never dereferences a live game control or reads text values.
inline void TraceGraph(Scope& state) {
#ifndef __SWITCH__
  static const bool enabled = std::getenv("E2E515_FOCUS_TRACE") != nullptr;
  if (!enabled) return;
  std::ostringstream snapshot;
  snapshot << "controller graph scope=" << state.serial << " selected=" << state.selected
           << " count=" << state.entries.size() << '\n';
  for (const auto& entry : state.entries) {
    std::string name;
    for (auto c : static_cast<std::u16string_view>(entry.control->ControlName)) {
      if (name.size() >= 64) break;
      name += c >= '!' && c <= '~' ? static_cast<char>(c) : '_';
    }
    if (name.empty() && DialogChoice(entry.control)) name = "DialogChoice" + std::to_string(entry.node.y);
    snapshot << "controller node scope=" << state.serial << " id=" << entry.node.id
             << " box=" << entry.node.x << ',' << entry.node.y << ',' << entry.node.width << ',' << entry.node.height
             << " name=" << name << '\n';
  }
  const auto text = snapshot.str();
  if (text == state.trace_snapshot) return;
  char done[96]{};
  std::snprintf(done, sizeof(done), "controller graph ready scope=%llu", static_cast<unsigned long long>(state.serial));
  if (const auto* path = std::getenv("E2E515_FOCUS_SNAPSHOT")) {
    const auto temporary = std::string(path) + ".tmp";
    {
      std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
      file << text << done << '\n'; file.flush();
      if (!file) { e2e_stage::Log("FAIL HOST focus snapshot write"); return; }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) { e2e_stage::Log("FAIL HOST focus snapshot publish"); return; }
    // The control protocol reads a single atomic snapshot, never a partially
    // appended log record. Keep only its summary in the human-readable journal.
    e2e_stage::Log(text.substr(0, text.find('\n')).c_str());
    e2e_stage::Log(done);
    state.trace_snapshot = text;
    return;
  }
  state.trace_snapshot = text;
  std::istringstream lines(text);
  for (std::string line; std::getline(lines, line);) e2e_stage::Log(line.c_str());
  e2e_stage::Log(done);
#endif
}

inline void Refresh(GI_MessageLoop::TMessageLoopGI* loop) {
  if (!active || active->owner != loop) return;
  auto& state = *active;
  const auto revision = controller_profiles::revision.load();
  if (revision != state.profile_revision) {
    state.focus = controller_profiles::Get().profile == controller_profiles::Profile::Hybrid;
    state.profile_revision = revision;
  }
  // Recovery controls must remain directly usable even in the Cursor profile.
  if (PreferencesFocus(loop)) state.focus = true;
  state.entries.clear(); state.graph.clear();
  if (AllowedContext(input_platform::GetInputContext())) Collect(loop->ContentPanel ? loop->ContentPanel : loop->RootUiObject, loop, state.entries);
  for (const auto& entry : state.entries) state.graph.push_back(entry.node);
  if (!controller_focus::Find(state.graph, state.selected)) {
    state.selected = controller_focus::First(state.graph);
    if (input_platform::GetInputContext() == Context::MainMenu)
      for (const auto& entry : state.entries) if (entry.control->ControlName == u"New") { state.selected = entry.node.id; break; }
    if (pas::class_cast_if<fCount2::TfCount2*>(loop))
      for (const auto& entry : state.entries) if (entry.control->ControlName == u"Ok") { state.selected = entry.node.id; break; }
  }
  input_platform::SetUiFocusPolicy(!state.entries.empty(), state.focus);
  TraceGraph(state);
}
inline void InvalidateFocus(GI_MessageLoop::TMessageLoopGI* loop, std::uint64_t id) {
  if (!active) return;
  if (const auto* node = controller_focus::Find(active->graph, id))
    loop->QueueUpdateRect({node->x - 3, node->y - 3, node->x + node->width + 3, node->y + node->height + 3});
}
inline void Activate(GI_MessageLoop::TObjectGI* object) {
  if (auto* shop = GoodsRow(object)) {
    if (!shop->CursorControl) return;
    const auto& box = object->HitTestBounds;
    const WindowsSdk::TPoint point{box.Left + (box.Right - box.Left) / 2, box.Top + (box.Bottom - box.Top) / 2};
    auto* cursor = shop->CursorControl;
    const auto previous = cursor->LocalPosition;
    pas::ScopeExit restore = [cursor, previous]() noexcept { cursor->SetPosition(previous); };
#if defined(E2E5_HOST_SDL_WINDOW)
    e2e515_save_checkpoint::ObserveCurrentWorld("trade-quantity-before");
#endif
    // The original handler uses GetCursorPoint as well as its Point argument.
    // Its stock, money/cargo limits, quantity dialog and transaction stay intact.
    cursor->SetPosition(point);
    try {
      shop->GoodsMouseUp(shop->GetByName(u"MainPanel"sv), 0, point);
    } catch (...) {
#if defined(E2E5_HOST_SDL_WINDOW)
      // Original mouse handlers finish with this control-flow exception.
      if (pas::class_cast_if<BreakMessageGIException::EBreakMessageGI*>(pas::caught_object()))
        e2e515_save_checkpoint::ObserveCurrentWorld("trade-quantity-after");
#endif
      throw; // Keep the original message-loop exception boundary.
    }
#if defined(E2E5_HOST_SDL_WINDOW)
    e2e515_save_checkpoint::ObserveCurrentWorld("trade-quantity-after");
#endif
    return;
  }
  if (DialogChoice(object)) {
    const auto& box = object->HitTestBounds;
    const WindowsSdk::TPoint point{box.Left + (box.Right - box.Left) / 2, box.Top + (box.Bottom - box.Top) / 2};
    // These two audited handlers are visual press then the original choice
    // transaction. The up handler may rebuild/free the row; do not touch it again.
    object->LeftButtonDownCallback(object, 0, point);
    object->LeftButtonUpCallback(object, 0, point);
    return;
  }
  if (auto* edit = pas::class_cast_if<GI_Edit::TEditGI*>(object)) {
    if (edit->MessageLoop) {
      // Mouse input selects the save-slot parent before entering its edit.
      // Keep that original transaction when semantic focus activates the edit.
      auto* parent = edit->Parent;
      auto* save = pas::class_cast_if<fSaveManager::TfSaveManager*>(edit->MessageLoop);
      if (save && parent && save->SelectedSlot != parent->UserValue &&
          parent->Parent && parent->Parent->ControlName == u"PanelSlot" &&
          pas::assigned(parent->LeftButtonDownCallback)) {
        const auto& box = edit->HitTestBounds;
        parent->LeftButtonDownCallback(parent, 0,
            {box.Left + (box.Right - box.Left) / 2, box.Top + (box.Bottom - box.Top) / 2});
      }
      edit->MessageLoop->SetFocusedControl(edit);
    }
    controller_text_entry::Open(edit);
    return;
  }
  if (auto* button = pas::class_cast_if<GI_GraphButton::TGraphButtonGI*>(object)) {
    if (button->Kind == GI_GraphButton::gbkFix || button->Kind == GI_GraphButton::gbkFixDisable) {
      button->Down = !button->Down;
      button->UpdateStateVisuals();
      if (button->Down && pas::assigned(button->DownCallback)) button->DownCallback(button);
      else if (!button->Down && pas::assigned(button->UpCallback)) button->UpCallback(button);
    } else if (pas::assigned(button->UpCallback)) button->UpCallback(button);
    else if (pas::assigned(button->DownCallback)) button->DownCallback(button);
    button->ExecuteOnPressCode();
    return;
  }
  if (pas::assigned(object->LeftButtonDownCallback)) {
    const auto& box = object->HitTestBounds;
    object->LeftButtonDownCallback(object, 0, {box.Left + (box.Right - box.Left) / 2, box.Top + (box.Bottom - box.Top) / 2});
  }
}

struct Preferences { bool active{}; unsigned row{}; controller_profiles::Settings pending; };
inline thread_local Preferences preferences;
inline bool PreferencesFocus(GI_MessageLoop::TMessageLoopGI* loop) {
  return preferences.active && pas::class_cast_if<GI_MessageBox::TMessageBoxGI*>(loop);
}
inline std::u16string Number(int value) { const auto str = std::to_string(value); return {str.begin(), str.end()}; }
inline pas::WideString Summary() {
  const auto& p = preferences.pending;
  const std::u16string yes = u"Вкл", no = u"Выкл";
  std::u16string text = u"Контроллер: D-pad выбирает/изменяет\n";
  text += controller_profiles::Get().swap_confirm_cancel ? u"B: сохранить; A: отмена; X: сброс\n\n" : u"A: сохранить; B: отмена; X: сброс\n\n";
  const std::u16string rows[]{std::u16string(u"Профиль: ") + (p.profile == controller_profiles::Profile::Hybrid ? u"Гибридный" : u"Курсорный"),
      u"Мёртвая зона: " + Number(p.deadzone), u"Скорость курсора: " + Number(static_cast<int>(p.pointer_speed)),
      u"Инверсия X: " + (p.invert_x ? yes : no), u"Инверсия Y: " + (p.invert_y ? yes : no),
      u"Ускорение: " + (p.acceleration ? yes : no), u"Поменять A/B: " + (p.swap_confirm_cancel ? yes : no)};
  for (unsigned i = 0; i < 7; ++i) text += (i == preferences.row ? u"> " : u"  ") + rows[i] + u"\n";
  return pas::WideString(text.c_str());
}
inline GI_Label::TLabelGI* FirstLabel(GI_MessageLoop::TObjectGI* root) {
  if (!root) return nullptr;
  if (auto* label = pas::class_cast_if<GI_Label::TLabelGI*>(root)) return label;
  for (auto* child = root->FirstChild; child; child = child->NextSibling)
    if (auto* label = FirstLabel(child)) return label;
  return nullptr;
}
inline void Adjust(int direction) {
  auto& p = preferences.pending;
  switch (preferences.row) {
    case 0: p.profile = p.profile == controller_profiles::Profile::Hybrid ? controller_profiles::Profile::Cursor : controller_profiles::Profile::Hybrid; break;
    case 1: p.deadzone = std::clamp(p.deadzone + direction * 1000, 0, 30000); break;
    case 2: {
      const int speeds[]{350, 650, 1000};
      int selected = direction > 0 ? speeds[0] : speeds[2];
      if (direction > 0) { for (int speed : speeds) if (speed > p.pointer_speed) { selected = speed; break; } }
      else { for (int i = 2; i >= 0; --i) if (speeds[i] < p.pointer_speed) { selected = speeds[i]; break; } }
      p.pointer_speed = selected; break;
    }
    case 3: p.invert_x = !p.invert_x; break;
    case 4: p.invert_y = !p.invert_y; break;
    case 5: p.acceleration = !p.acceleration; break;
    case 6: p.swap_confirm_cancel = !p.swap_confirm_cancel; break;
  }
}
inline void OpenPreferences(GI_MessageLoop::TMessageLoopGI* parent) {
  if (preferences.active) return;
  preferences.active = true; preferences.row = 0; preferences.pending = controller_profiles::Get();
  pas::ScopeExit close = []() noexcept { preferences.active = false; };
  const auto result = GI_MessageBox::ShowMessageBoxGI(parent, Summary(), GI_MessageBox::mbgOK | GI_MessageBox::mbgCancel, 0, 0, 0);
  if (result == GI_MessageBox::mbgResultOK) {
    controller_profiles::Set(preferences.pending);
    if (!SaveSettings(preferences.pending)) {
      e2e_stage::Log("controller settings persistence failed; in-memory settings retained");
      preferences.active = false;
      GI_MessageBox::ShowMessageBoxGI(parent,
          pas::WideString(u"Не удалось сохранить параметры контроллера. Они действуют до выхода из игры. Повторите сохранение позже."),
          GI_MessageBox::mbgOK, 0, 0, 0);
    }
  }
}

inline bool Process(GI_MessageLoop::TMessageLoopGI* loop, std::uint32_t message,
                    std::uint32_t wparam, std::int32_t lparam) {
  if (message != controller_messages::kMessage) return false;
  if (!active || active->owner != loop || wparam >= static_cast<unsigned>(Action::Count)) return true;
  const auto action = static_cast<Action>(wparam);
  const auto edge = static_cast<Edge>(lparam);
  if (action == Action::ToggleNavigationMode) {
    InvalidateFocus(loop, active->selected);
    active->focus = edge == Edge::FocusMode;
    win32_compat::CancelActivePointerForMode(loop);
    InvalidateFocus(loop, active->selected);
    return true;
  }
  if (edge != Edge::Pressed && edge != Edge::Repeat) return true;
  if (action == Action::OpenControllerSettings) { if (edge == Edge::Pressed) OpenPreferences(loop); return true; }
  if (preferences.active) if (auto* dialog = pas::class_cast_if<GI_MessageBox::TMessageBoxGI*>(loop)) {
    if (action == Action::Confirm) dialog->AcceptClick(nullptr);
    else if (action == Action::Cancel) dialog->CancelClick(nullptr);
    else {
      if (action == Action::NavigateUp) preferences.row = (preferences.row + 6) % 7;
      if (action == Action::NavigateDown) preferences.row = (preferences.row + 1) % 7;
      if (action == Action::NavigateLeft || action == Action::PrevTab) Adjust(-1);
      if (action == Action::NavigateRight || action == Action::NextTab) Adjust(1);
      if (action == Action::Inspect) { preferences.pending = controller_profiles::Settings{}; preferences.row = 0; }
      if (auto* label = FirstLabel(loop->ContentPanel)) label->SetText(Summary());
      loop->InvalidateViewport();
    }
    return true;
  }
  if (auto* quantity = pas::class_cast_if<fCount2::TfCount2*>(loop)) {
    if (action == Action::NavigateLeft || action == Action::NavigateRight) {
      // Use the original bounded single-step operation; unlike mouse-down it
      // does not create a repeat timer needing a later mouse-up.
      quantity->MainPanelKeyDown(quantity->GetByName(u"MainPanel"sv),
          action == Action::NavigateLeft ? WindowsSdk::VK_LEFT : WindowsSdk::VK_RIGHT);
      char diagnostic[160]{};
      std::snprintf(diagnostic, sizeof(diagnostic), "controller quantity value=%d minimum=%d maximum=%d limit=%d",
          quantity->Value, quantity->Minimum, quantity->Maximum, quantity->Limit);
      e2e_stage::Log(diagnostic);
      return true;
    }
  }
  Refresh(loop);
  if (active->entries.empty()) return true;
  using D = controller_focus::Direction;
  if (action == Action::NavigateUp || action == Action::NavigateDown ||
      action == Action::NavigateLeft || action == Action::NavigateRight ||
      action == Action::PrevTab || action == Action::NextTab) {
    InvalidateFocus(loop, active->selected);
    const D direction = action == Action::NavigateUp ? D::Up : action == Action::NavigateDown ? D::Down :
        action == Action::NavigateLeft || action == Action::PrevTab ? D::Left : D::Right;
    active->selected = controller_focus::Navigate(active->graph, active->selected, direction);
    InvalidateFocus(loop, active->selected);
    return true;
  }
  for (const auto& entry : active->entries) if (entry.node.id == active->selected) {
    char diagnostic[160]{};
    std::snprintf(diagnostic, sizeof(diagnostic), "controller native action=%s target=%llx edge=%d",
        controller_actions::Name(action), static_cast<unsigned long long>(entry.node.id), static_cast<int>(edge));
    e2e_stage::Log(diagnostic);
    if (action == Action::Confirm && edge == Edge::Pressed) Activate(entry.control);
    else if (action == Action::Inspect && pas::assigned(entry.control->HelpCallback)) entry.control->HelpCallback(entry.control, false);
    else if (action == Action::OpenContextAction && pas::assigned(entry.control->RightButtonDownCallback))
      entry.control->RightButtonDownCallback(entry.control, 0, {entry.node.x + entry.node.width / 2, entry.node.y + entry.node.height / 2});
    break;
  }
  return true;
}

inline void Draw(GI_MessageLoop::TMessageLoopGI* loop) {
  if (!active || active->owner != loop || !input_platform::UiFocusMode()) return;
  const auto* node = controller_focus::Find(active->graph, active->selected);
  auto* buffer = GR_Main::ScreenRenderBuffer;
  if (!node || !buffer || !buffer->GetPixels() || buffer->Width <= 0 || buffer->Height <= 0 ||
      buffer->PitchBytes < buffer->Width * 2) return;
  const auto left = std::clamp(node->x - 2, 0, buffer->Width - 1);
  const auto top = std::clamp(node->y - 2, 0, buffer->Height - 1);
  const auto right = std::clamp(node->x + node->width + 1, 0, buffer->Width - 1);
  const auto bottom = std::clamp(node->y + node->height + 1, 0, buffer->Height - 1);
  for (int y = top; y <= bottom; ++y) {
    auto* row = reinterpret_cast<std::uint16_t*>(static_cast<unsigned char*>(buffer->GetPixels()) + std::size_t(y) * buffer->PitchBytes);
    if (y == top || y == bottom) for (int x = left; x <= right; ++x) row[x] = 0xffe0;
    else { row[left] = 0xffe0; row[right] = 0xffe0; }
  }
}
} // namespace srhd_awa::platform::controller_ui

#include "ui_input.hpp"

#include "ui_edit.hpp"
#include "ui_graph_button.hpp"
#include "ui_scroll_bar.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_zone.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>
#include <utility>

namespace srhd_awa::platform::ui {
namespace {
bool Within(const UiObject* object, const UiObject* ancestor) {
  for (auto* current = object; current; current = current->Parent())
    if (current == ancestor) return true;
  return false;
}
std::string Path(const UiObject* object) {
  if (!object) return {};
  const auto* parent = object->Parent();
  const auto prefix = parent ? Path(parent) + "/" : std::string{};
  if (!object->Name().empty()) return prefix + object->Name();
  std::size_t ordinal{};
  if (parent) for (const auto& sibling : parent->Children()) {
    if (sibling.get() == object) break;
    if (sibling->Kind() == object->Kind()) ++ordinal;
  }
  return prefix + std::to_string(static_cast<int>(object->Kind())) + "#" + std::to_string(ordinal);
}
std::int32_t Bounded(std::int64_t value) {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(value,
      std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}
}  // namespace

std::vector<std::uint8_t> SerializeActions(const std::vector<UiAction>& actions) {
  std::vector<std::uint8_t> bytes;
  const auto u32 = [&bytes](std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
  };
  const auto u64 = [&bytes](std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
  };
  const auto string = [&bytes, &u32](const std::string& value) {
    u32(static_cast<std::uint32_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
  };
  u32(static_cast<std::uint32_t>(actions.size()));
  for (const auto& action : actions) {
    u64(action.sequence);
    u32(static_cast<std::uint32_t>(action.kind));
    string(action.control);
    u32(static_cast<std::uint32_t>(action.point.x));
    u32(static_cast<std::uint32_t>(action.point.y));
    u32(static_cast<std::uint32_t>(action.value));
    string(action.payload);
    u32(static_cast<std::uint32_t>(action.detail));
    u32(static_cast<std::uint32_t>(action.old_state));
    u32(static_cast<std::uint32_t>(action.new_state));
  }
  return bytes;
}
UiActionFingerprint FingerprintActions(const std::vector<UiAction>& actions) {
  const auto bytes = SerializeActions(actions);
  std::uint32_t crc = 0xffffffffu;
  std::uint64_t fnv = 0xcbf29ce484222325ull;
  for (auto byte : bytes) {
    crc ^= byte;
    for (unsigned i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    fnv = (fnv ^ byte) * 0x100000001b3ull;
  }
  return {crc ^ 0xffffffffu, fnv, bytes.size()};
}

UiInputRouter::UiInputRouter(UiObject& root) : root_(&root), root_lifetime_(root.Lifetime()) {
  root.SetMutationObserver([this](UiObject* subtree) { ForgetSubtree(subtree); });
}
UiInputRouter::~UiInputRouter() {
  if (!root_lifetime_.expired()) root_->SetMutationObserver({});
}
bool UiInputRouter::OwnedAndActive(const UiObject* control) const {
  if (!control || !Within(control, root_)) return false;
  for (auto* current = control; current; current = current->Parent())
    if (!current->Active()) return false;
  return true;
}
void UiInputRouter::Emit(UiActionKind kind, const UiObject* control,
                         std::int32_t value, std::string payload, std::int32_t detail,
                         std::int32_t old_state, std::int32_t new_state) {
  actions_.push_back({++sequence_, kind, Path(control), pointer_, value,
                      std::move(payload), detail, old_state, new_state});
}
void UiInputRouter::SetHoveredControl(UiObject* control) {
  if (control && !OwnedAndActive(control)) return;
  if (hovered_ == control) return;
  if (hovered_) {
    auto* old = hovered_;
    hovered_ = nullptr;
    old->OnHoverLost();
    if (auto* button = dynamic_cast<UiGraphButton*>(old)) {
      button->SetHovered(false);
      if (button->Down() && (button->ButtonKind() == GraphButtonKind::Normal ||
                             button->ButtonKind() == GraphButtonKind::Disable)) {
        button->SetDown(false);
        Emit(UiActionKind::ButtonUp, old);
      }
      if (!button->Disabled() && !button->SoundLeave().empty())
        Emit(UiActionKind::SoundRequested, old, 0, button->SoundLeave());
    }
    Emit(UiActionKind::HoverLost, old, 0, {}, 0, 1, 0);
  }
  hovered_ = control;
  if (hovered_) {
    hovered_->OnHoverGained();
    if (auto* button = dynamic_cast<UiGraphButton*>(hovered_)) {
      button->SetHovered(true);
      if (!button->Disabled() && !button->SoundEnter().empty())
        Emit(UiActionKind::SoundRequested, button, 0, button->SoundEnter());
    }
    Emit(UiActionKind::HoverGained, hovered_, 0, {}, 0, 0, 1);
    if (help_.empty() && !hovered_->HelpText().empty()) {
      help_ = hovered_->HelpText();
      Emit(UiActionKind::HelpShown, hovered_, 0, help_);
    }
  }
}
void UiInputRouter::SetFocusedControl(UiObject* control) {
  if (control && !OwnedAndActive(control)) return;
  if (focused_ == control) return;
  if (focused_) {
    auto* old = focused_;
    old->OnFocusLost();
    if (auto* edit = dynamic_cast<UiEdit*>(old)) edit->SetFocused(false);
    Emit(UiActionKind::FocusLost, old, 0, {}, 0, 1, 0);
  }
  focused_ = control;
  caret_elapsed_ = 0;
  if (focused_) {
    focused_->OnFocusGained();
    if (auto* edit = dynamic_cast<UiEdit*>(focused_)) { edit->SetFocused(true); edit->SetCaretBlink(true); }
    Emit(UiActionKind::FocusGained, focused_, 0, {}, 0, 0, 1);
  }
}
void UiInputRouter::ForgetSubtree(UiObject* subtree) {
  if (pressed_ && Within(pressed_, subtree)) {
    if (auto* bar = dynamic_cast<UiScrollBar*>(pressed_)) bar->ReleaseRegion();
    pressed_ = nullptr;
    repeat_elapsed_ = 0;
    repeat_started_ = false;
  }
  if (subtree->MouseInside()) Leave(*subtree);
  else for (const auto& child : subtree->Children())
    if (child->MouseInside()) Leave(*child);
  if (hovered_ && Within(hovered_, subtree)) SetHoveredControl(nullptr);
  if (focused_ && Within(focused_, subtree)) SetFocusedControl(nullptr);
  if (subtree == root_) root_inside_ = false;
  subtree->SetMouseInside(false);
  if (auto* zone = dynamic_cast<UiZone*>(subtree)) zone->SetCursorInside(false);
}
void UiInputRouter::Enter(UiObject& control) {
  control.OnMouseEnter();
  control.SetMouseInside(true);
  Emit(UiActionKind::MouseEnter, &control);
  if (control.HasMouseEnterCode())
    Emit(UiActionKind::DeferredScriptRequested, &control, 0, "OnMouseEnterCode");
  if (!control.HelpText().empty()) {
    help_ = control.HelpText();
    Emit(UiActionKind::HelpShown, &control, 0, help_);
  }
}
void UiInputRouter::Leave(UiObject& control) {
  control.OnMouseLeave();
  control.SetMouseInside(false);
  if (auto* zone = dynamic_cast<UiZone*>(&control); zone && zone->CursorInside()) {
    zone->SetCursorInside(false);
    Emit(UiActionKind::NamedEvent, zone, 0, "zone-leave");
  }
  if (auto* bar = dynamic_cast<UiScrollBar*>(&control); bar && bar->PressedRegion() == 0)
    bar->SetHoveredRegion(0);
  Emit(UiActionKind::MouseLeave, &control);
  if (control.HasMouseLeaveCode())
    Emit(UiActionKind::DeferredScriptRequested, &control, 0, "OnMouseLeaveCode");
  for (const auto& child : control.Children())
    if (child->MouseInside()) Leave(*child);
  if (!control.HelpText().empty() && help_ == control.HelpText()) {
    help_.clear(); Emit(UiActionKind::HelpCleared, &control);
  }
  if (hovered_ == &control) SetHoveredControl(nullptr);
}
void UiInputRouter::Move(UiObject& control, Point point) {
  control.ProcessPointerMove(point);
  HandleControlMove(control, point);
  for (const auto& child : control.Children())
    if (child->Active() && !child->ContainsPoint(point) && child->MouseInside()) Leave(*child);
  for (const auto& child : control.Children()) {
    if (!child->Active() || !child->ContainsPoint(point)) continue;
    if (!child->MouseInside()) Enter(*child);
    Move(*child, point);
  }
}
void UiInputRouter::PointerMove(Point point, UiModifiers modifiers) {
  pointer_ = point; modifiers_ = modifiers;
  // Upstream sends the focused control a move before traversing the root.
  if (focused_ && OwnedAndActive(focused_)) {
    focused_->ProcessPointerMove(point);
    HandleControlMove(*focused_, point);
  }
  if (root_->ContainsPoint(point)) {
    if (!root_inside_) { Enter(*root_); root_inside_ = true; }
    Move(*root_, point);
  } else if (root_inside_) {
    Leave(*root_); root_inside_ = false;
  }
}
void UiInputRouter::PointerLeave() {
  if (root_inside_) { Leave(*root_); root_inside_ = false; }
  SetHoveredControl(nullptr);
}
void UiInputRouter::DispatchButton(UiObject& control, UiPointerButton button, bool down, Point point) {
  if (button == UiPointerButton::Left) {
    if (down) control.ProcessLeftButtonDown(point);
    else control.ProcessLeftButtonUp(point);
  } else {
    if (down) control.ProcessRightButtonDown(point);
    else control.ProcessRightButtonUp(point);
  }
  HandleControlButton(control, button, down, point);
  for (const auto& child : control.Children())
    if (child->Active() && child->ContainsPoint(point)) DispatchButton(*child, button, down, point);
}
void UiInputRouter::PointerDown(UiPointerButton button, Point point, UiModifiers modifiers) {
  pointer_ = point; modifiers_ = modifiers;
  if (button == UiPointerButton::Left) left_down_ = true;
  else right_down_ = true;
  if (root_->ContainsPoint(point)) DispatchButton(*root_, button, true, point);
}
void UiInputRouter::PointerUp(UiPointerButton button, Point point, UiModifiers modifiers) {
  pointer_ = point; modifiers_ = modifiers;
  if (button == UiPointerButton::Left) left_down_ = false;
  else right_down_ = false;
  // Focused control receives left-up first, as in the window message path.
  if (button == UiPointerButton::Left && focused_ && OwnedAndActive(focused_)) {
    focused_->ProcessLeftButtonUp(point);
    HandleControlButton(*focused_, button, false, point);
  }
  if (root_->ContainsPoint(point)) DispatchButton(*root_, button, false, point);
  if (button == UiPointerButton::Left) { pressed_ = nullptr; repeat_elapsed_ = 0; repeat_started_ = false; }
}
void UiInputRouter::PointerDoubleClick(UiPointerButton button, Point point, UiModifiers modifiers) {
  PointerDown(button, point, modifiers);
  if (root_->ContainsPoint(point)) DispatchDoubleClick(*root_, point);
}
void UiInputRouter::DispatchDoubleClick(UiObject& control, Point point) {
  control.ProcessDoubleClick(point);
  for (const auto& child : control.Children())
    if (child->Active() && child->ContainsPoint(point)) DispatchDoubleClick(*child, point);
}
std::int32_t UiInputRouter::QueryPointOcclusionState(Point point, const UiObject* ignore,
                                                       const UiObject* start) const {
  if (!start) start = root_;
  if (!start) return 0;
  if (!start->Parent() || start->ContainsPoint(point)) {
    // children_[0] is FirstChild; reverse order matches LastChild->PrevSibling.
    for (auto it = start->Children().rbegin(); it != start->Children().rend(); ++it) {
      const auto result = QueryPointOcclusionState(point, ignore, it->get());
      if (result) return result;
    }
    if (start->MouseBlocking() && start != ignore) return 1;
  }
  return start == ignore ? -1 : 0;
}

void UiInputRouter::ChangedScroll(UiScrollBar& bar, std::int32_t old_position) {
  if (bar.Position() != old_position)
    Emit(UiActionKind::ScrollPositionChanged, &bar, bar.Position(), {}, 0, old_position, bar.Position());
}
std::int32_t UiInputRouter::ScrollDragOffset(const UiScrollBar& bar, Point point) const {
  const bool horizontal = bar.Orientation() == 1;
  const auto axis = [horizontal](const UiObject* image) -> std::int32_t {
    if (!image) return 0;
    return horizontal ? image->ClientSize().width : image->ClientSize().height;
  };
  const auto normal = ScrollBarState::Normal;
  const auto up = axis(bar.Image(ScrollBarPart::Up, normal));
  const auto down = axis(bar.Image(ScrollBarPart::Down, normal));
  const auto length = horizontal ? bar.ClientSize().width : bar.ClientSize().height;
  auto denominator = length - up - down;
  if (bar.CalculationMode() == 0)
    denominator -= axis(bar.Image(ScrollBarPart::ThumbCenter, normal)) +
                   axis(bar.Image(ScrollBarPart::ThumbTop, normal)) +
                   axis(bar.Image(ScrollBarPart::ThumbBottom, normal));
  if (denominator <= 0) return 0;
  const auto bounds = bar.HitTestBounds();
  const auto coordinate = (horizontal ? point.x - bounds.left : point.y - bounds.top) - up;
  const auto range = static_cast<std::int64_t>(bar.Maximum()) - bar.Minimum();
  return Bounded(static_cast<std::int64_t>(std::trunc(
      static_cast<long double>(range) * coordinate / denominator)));
}
void UiInputRouter::HandleControlMove(UiObject& control, Point point) {
  if (auto* button = dynamic_cast<UiGraphButton*>(&control)) {
    if (button->MouseBlockingTest() &&
        QueryPointOcclusionState(button->AbsolutePosition(), button) == 1) return;
    if (button->HitTest(point)) {
      if (!button->Disabled()) SetHoveredControl(button);
    } else if (hovered_ == button) SetHoveredControl(nullptr);
  } else if (auto* bar = dynamic_cast<UiScrollBar*>(&control)) {
    if (bar->PressedRegion() == 5) {
      const auto old = bar->Position();
      bar->SetPosition(Bounded(static_cast<std::int64_t>(drag_start_position_) + ScrollDragOffset(*bar, point)));
      ChangedScroll(*bar, old);
    }
    bar->SetHoveredRegion(bar->GetHitRegion(bar->ToLocalPoint(point)));
  } else if (auto* zone = dynamic_cast<UiZone*>(&control)) {
    // Zone-specific circle hit test can differ from tree bounds.
    const bool inside = zone->HitTest(point);
    if (inside != zone->CursorInside()) {
      zone->SetCursorInside(inside);
      Emit(UiActionKind::NamedEvent, zone, 0, inside ? "zone-enter" : "zone-leave");
    }
  }
}
void UiInputRouter::HandleControlButton(UiObject& control, UiPointerButton button,
                                         bool down, Point point) {
  if (button == UiPointerButton::Right) {
    if (!down && control.HasRightClickCode())
      Emit(UiActionKind::DeferredScriptRequested, &control, 0, "OnMouseRightClick");
    return;
  }
  if (auto* graph = dynamic_cast<UiGraphButton*>(&control)) {
    if (!graph->HitTest(point) || (graph->MouseBlockingTest() &&
        QueryPointOcclusionState(point, graph) == 1)) return;
    if (down) {
      if (graph->Disabled() && (graph->ButtonKind() == GraphButtonKind::Disable ||
                                graph->ButtonKind() == GraphButtonKind::FixDisable)) return;
      if (hovered_ != graph) return;
      const bool fixed = graph->ButtonKind() == GraphButtonKind::Fix ||
                         graph->ButtonKind() == GraphButtonKind::FixDisable;
      const bool was_down = graph->Down();
      graph->SetDown(fixed ? !graph->Down() : true);
      pressed_ = graph;
      Emit(UiActionKind::ButtonDown, graph, graph->Down(), {}, 0, was_down, graph->Down());
      if (!graph->SoundClick().empty()) Emit(UiActionKind::SoundRequested, graph, 0, graph->SoundClick());
    } else if (pressed_ == graph) {
      const bool fixed = graph->ButtonKind() == GraphButtonKind::Fix ||
                         graph->ButtonKind() == GraphButtonKind::FixDisable;
      const bool was_down = graph->Down();
      if (!fixed) graph->SetDown(false);
      Emit(UiActionKind::ButtonUp, graph, graph->Down(), {}, 0, was_down, graph->Down());
      Emit(UiActionKind::ButtonActivate, graph);
      if (graph->HasOnPressCode())
        Emit(UiActionKind::DeferredScriptRequested, graph, 0, "OnPressCode");
      pressed_ = nullptr;
    }
  } else if (auto* edit = dynamic_cast<UiEdit*>(&control)) {
    if (down) SetFocusedControl(edit);
  } else if (auto* zone = dynamic_cast<UiZone*>(&control)) {
    if (zone->HitTest(point)) {
      if (!zone->CursorInside()) { zone->SetCursorInside(true); Emit(UiActionKind::NamedEvent, zone, 0, "zone-enter"); }
      Emit(down ? UiActionKind::ZoneDown : UiActionKind::ZoneUp, zone);
    }
  } else if (auto* bar = dynamic_cast<UiScrollBar*>(&control)) {
    if (down) {
      SetFocusedControl(bar);
      const auto region = bar->GetHitRegion(bar->ToLocalPoint(point));
      bar->SetHoveredRegion(region);
      const auto old = bar->Position();
      bar->PressRegion(region);
      ChangedScroll(*bar, old);
      pressed_ = bar;
      if (region == 5) drag_start_position_ = Bounded(
          static_cast<std::int64_t>(bar->Position()) - ScrollDragOffset(*bar, point));
      repeat_elapsed_ = 0; repeat_started_ = false;
    } else {
      bar->ReleaseRegion();
      if (focused_ == bar) SetFocusedControl(nullptr);
    }
  }
}

void UiInputRouter::KeyDown(UiKey key, UiModifiers modifiers) {
  modifiers_ = modifiers;
  BroadcastKey(*root_, key, true);
  if (!focused_ || !OwnedAndActive(focused_)) return;
  focused_->ProcessKeyDown(static_cast<std::int32_t>(key));
  auto* edit = dynamic_cast<UiEdit*>(focused_);
  if (!edit) return;
  const auto old = edit->Text();
  switch (key) {
    case UiKey::Backspace:
      if (modifiers.shift) edit->SetText({}); else edit->Backspace(); break;
    case UiKey::Delete: edit->Delete(); break;
    case UiKey::Left: edit->MoveLeft(); break;
    case UiKey::Right: edit->MoveRight(); break;
    case UiKey::Home: edit->Home(); break;
    case UiKey::End: edit->End(); break;
    case UiKey::Enter: if (edit->ClearFocusOnEnter()) SetFocusedControl(nullptr); break;
    case UiKey::Tab: {
      if (auto* parent = edit->Parent()) {
        const auto& siblings = parent->Children();
        auto it = std::find_if(siblings.begin(), siblings.end(), [edit](const auto& p) { return p.get() == edit; });
        if (it != siblings.end()) {
          if (modifiers.shift) {
            for (++it; it != siblings.end(); ++it)
              if (auto* next = dynamic_cast<UiEdit*>(it->get()); next && next->Active()) {
                SetFocusedControl(next); break;
              }
          } else {
            while (it != siblings.begin()) {
              --it;
              if (auto* next = dynamic_cast<UiEdit*>(it->get()); next && next->Active()) {
                SetFocusedControl(next); break;
              }
            }
          }
        }
      }
      break;
    }
    default: break;
  }
  if (edit->Text() != old) Emit(UiActionKind::EditTextChanged, edit,
      static_cast<std::int32_t>(edit->Text().size()), {}, 0,
      static_cast<std::int32_t>(old.size()), static_cast<std::int32_t>(edit->Text().size()));
}
void UiInputRouter::KeyUp(UiKey key, UiModifiers modifiers) {
  modifiers_ = modifiers;
  BroadcastKey(*root_, key, false);
  if (focused_ && OwnedAndActive(focused_)) focused_->ProcessKeyUp(static_cast<std::int32_t>(key));
}
void UiInputRouter::BroadcastKey(UiObject& control, UiKey key, bool down) {
  if (down) control.OnKeyBroadcastDown(static_cast<std::int32_t>(key));
  else control.OnKeyBroadcastUp(static_cast<std::int32_t>(key));
  if (down && control.HasOnKeyCode())
    Emit(UiActionKind::DeferredScriptRequested, &control,
         static_cast<std::int32_t>(key), "OnKey");
  // Upstream broadcasts into every child, including inactive controls.
  for (const auto& child : control.Children()) BroadcastKey(*child, key, down);
}
void UiInputRouter::TextInput(char16_t character) {
  if (character < u' ' || !focused_ || !OwnedAndActive(focused_)) return;
  focused_->ProcessCharacter(character);
  if (auto* edit = dynamic_cast<UiEdit*>(focused_); edit) {
    const auto before = static_cast<std::int32_t>(edit->Text().size());
    if (edit->InsertCharacter(character))
      Emit(UiActionKind::EditTextChanged, edit, static_cast<std::int32_t>(edit->Text().size()), {},
           static_cast<std::int32_t>(character), before, static_cast<std::int32_t>(edit->Text().size()));
  }
}
void UiInputRouter::Update(std::uint64_t delta_ms) {
  if (auto* edit = dynamic_cast<UiEdit*>(focused_); edit && OwnedAndActive(edit)) {
    caret_elapsed_ = std::min<std::uint64_t>(caret_elapsed_ + std::min(delta_ms, UINT64_MAX - caret_elapsed_), UINT64_MAX);
    if (caret_elapsed_ > 200) {
      // The upstream loop resets LastCaretTick to the current tick and
      // performs one toggle even when a frame is delayed by several periods.
      caret_elapsed_ = 0;
      edit->SetCaretBlink(!edit->CaretBlinkOn());
      edit->OnCaretBlink();
    }
  }
  auto* bar = dynamic_cast<UiScrollBar*>(pressed_);
  if (!left_down_ || !bar || !OwnedAndActive(bar) || bar->PressedRegion() < 1 || bar->PressedRegion() > 4) return;
  repeat_elapsed_ = std::min<std::uint64_t>(repeat_elapsed_ + std::min(delta_ms, UINT64_MAX - repeat_elapsed_), UINT64_MAX);
  if (!repeat_started_) {
    if (repeat_elapsed_ < 500) return;
    repeat_elapsed_ -= 500;
    repeat_started_ = true;
    const auto old = bar->Position(); bar->PressRegion(bar->PressedRegion()); ChangedScroll(*bar, old);
  }
  const auto repeats = std::min<std::uint64_t>(repeat_elapsed_ / 50, 100000);
  repeat_elapsed_ %= 50;
  for (std::uint64_t i = 0; i < repeats && pressed_ == bar; ++i) {
    const auto old = bar->Position(); bar->PressRegion(bar->PressedRegion()); ChangedScroll(*bar, old);
    if (old == bar->Position()) break;
  }
}

}  // namespace srhd_awa::platform::ui

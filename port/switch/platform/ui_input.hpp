#pragma once

#include "ui_object.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::ui {

enum class UiPointerButton { Left, Right };
enum class UiKey { Backspace, Delete, Left, Right, Up, Down, Home, End, Enter, Tab, Escape };
struct UiModifiers { bool shift{}, control{}, alt{}; };
enum class UiActionKind {
  MouseEnter, MouseLeave, HoverGained, HoverLost, FocusGained, FocusLost,
  ButtonDown, ButtonUp, ButtonActivate, ZoneDown, ZoneUp,
  ScrollPositionChanged, EditTextChanged, HelpShown, HelpCleared,
  NamedEvent, DeferredScriptRequested, SoundRequested
};
struct UiAction {
  std::uint64_t sequence{};
  UiActionKind kind{};
  std::string control;
  Point point{};
  std::int32_t value{};
  std::string payload;
  std::int32_t detail{};
  std::int32_t old_state{};
  std::int32_t new_state{};
};
struct UiActionFingerprint {
  std::uint32_t crc32{};
  std::uint64_t fnv64{};
  std::uint64_t bytes{};
};
std::vector<std::uint8_t> SerializeActions(const std::vector<UiAction>& actions);
UiActionFingerprint FingerprintActions(const std::vector<UiAction>& actions);

class UiInputRouter final {
 public:
  explicit UiInputRouter(UiObject& root);
  ~UiInputRouter();
  UiInputRouter(const UiInputRouter&) = delete;
  UiInputRouter& operator=(const UiInputRouter&) = delete;
  void PointerMove(Point point, UiModifiers modifiers = {});
  void PointerLeave();
  void PointerDown(UiPointerButton button, Point point, UiModifiers modifiers = {});
  void PointerUp(UiPointerButton button, Point point, UiModifiers modifiers = {});
  void PointerDoubleClick(UiPointerButton button, Point point, UiModifiers modifiers = {});
  void KeyDown(UiKey key, UiModifiers modifiers = {});
  void KeyUp(UiKey key, UiModifiers modifiers = {});
  void TextInput(char16_t character);
  void Update(std::uint64_t delta_ms);
  std::int32_t QueryPointOcclusionState(Point point, const UiObject* ignore = nullptr,
                                        const UiObject* start = nullptr) const;
  void SetHoveredControl(UiObject* control);
  void SetFocusedControl(UiObject* control);
  UiObject* HoveredControl() const { return hovered_; }
  UiObject* FocusedControl() const { return focused_; }
  Point PointerPosition() const { return pointer_; }
  bool RootMouseInside() const { return root_inside_; }
  bool LeftButtonDown() const { return left_down_; }
  bool RightButtonDown() const { return right_down_; }
  const std::string& CurrentHelp() const { return help_; }
  const std::vector<UiAction>& Actions() const { return actions_; }
  void ClearActions() { actions_.clear(); }

 private:
  void ForgetSubtree(UiObject* subtree);
  bool OwnedAndActive(const UiObject* control) const;
  void Emit(UiActionKind kind, const UiObject* control, std::int32_t value = 0,
            std::string payload = {}, std::int32_t detail = 0,
            std::int32_t old_state = 0, std::int32_t new_state = 0);
  void Enter(UiObject& control);
  void Leave(UiObject& control);
  void Move(UiObject& control, Point point);
  void DispatchButton(UiObject& control, UiPointerButton button, bool down, Point point);
  void DispatchDoubleClick(UiObject& control, Point point);
  void BroadcastKey(UiObject& control, UiKey key, bool down);
  void HandleControlMove(UiObject& control, Point point);
  void HandleControlButton(UiObject& control, UiPointerButton button, bool down, Point point);
  void ChangedScroll(class UiScrollBar& bar, std::int32_t old_position);
  std::int32_t ScrollDragOffset(const class UiScrollBar& bar, Point point) const;
  UiObject* root_{};
  std::weak_ptr<void> root_lifetime_;
  Point pointer_{};
  UiModifiers modifiers_{};
  UiObject* hovered_{};
  UiObject* focused_{};
  UiObject* pressed_{};
  bool root_inside_{}, left_down_{}, right_down_{};
  std::string help_;
  std::uint64_t sequence_{}, caret_elapsed_{}, repeat_elapsed_{};
  bool repeat_started_{};
  std::int32_t drag_start_position_{};
  std::vector<UiAction> actions_;
};

}  // namespace srhd_awa::platform::ui

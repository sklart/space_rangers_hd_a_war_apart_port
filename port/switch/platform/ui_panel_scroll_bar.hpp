#pragma once

#include "ui_scroll_bar.hpp"

#include <memory>
#include <string>

namespace srhd_awa::platform::ui {

class UiPanelScrollBar final : public UiPanel {
 public:
  UiPanelScrollBar();
  NodeKind Kind() const override { return NodeKind::PanelScrollBar; }
  UiScrollBar* HorizontalBar() const { return horizontal_; }
  UiScrollBar* VerticalBar() const { return vertical_; }
  void SetHorizontalActive(bool value);
  void SetVerticalActive(bool value);
  void SetExternal(bool value);
  void SetUnlimitedWorld(bool value) { unlimited_world_ = value; }
  void SetAutoHorizontal(bool value) { auto_horizontal_ = value; UpdateScrollbarPlacement(); }
  void SetAutoVertical(bool value) { auto_vertical_ = value; UpdateScrollbarPlacement(); }
  void SetHorizontalRect(Rect value) { horizontal_rect_ = value; UpdateScrollbarPlacement(); }
  void SetVerticalRect(Rect value) { vertical_rect_ = value; UpdateScrollbarPlacement(); }
  bool External() const { return external_; }
  bool UnlimitedWorld() const { return unlimited_world_; }
  bool AutoHorizontal() const { return auto_horizontal_; }
  bool AutoVertical() const { return auto_vertical_; }
  void FinalizeAfterAttach();
  bool UpdateScrollbarPlacement(std::string* error = nullptr);
  bool UpdateScrollRanges(std::string* error = nullptr);
  void SetScrollOffset(Point value);

 protected:
  Point ChildAbsolutePosition(const UiObject& child) const override;
  void OnGeometryChanged() override { UpdateScrollbarPlacement(); }

 private:
  void MoveBarsToExpectedParent();
  UiScrollBar* horizontal_{};
  UiScrollBar* vertical_{};
  Rect horizontal_rect_{}, vertical_rect_{};
  bool auto_horizontal_{true}, auto_vertical_{true};
  bool external_{}, unlimited_world_{true};
  std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

}  // namespace srhd_awa::platform::ui

#include "ui_panel_scroll_bar.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
std::int32_t Bound(std::int64_t value) {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(value,
      std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}
}  // namespace

UiPanelScrollBar* UiObject::AddPanelScrollBar() {
  auto child = std::make_unique<UiPanelScrollBar>();
  auto* result = child.get();
  Attach(std::move(child));
  result->FinalizeAfterAttach();
  return result;
}

UiPanelScrollBar::UiPanelScrollBar() {
  auto horizontal = std::make_unique<UiScrollBar>();
  auto vertical = std::make_unique<UiScrollBar>();
  horizontal_ = horizontal.get();
  vertical_ = vertical.get();
  horizontal_->SetOrientation(1);
  horizontal_->SetCalculationMode(1);
  horizontal_->SetActive(false);
  vertical_->SetOrientation(2);
  vertical_->SetCalculationMode(1);
  vertical_->SetActive(false);
  horizontal_->SetDepth(-1.0e30);
  vertical_->SetDepth(-1.0e30);
  Attach(std::move(horizontal));
  Attach(std::move(vertical));
  const std::weak_ptr<bool> alive = alive_;
  horizontal_->SetPositionChangedCallback([this, alive](std::int32_t) {
    if (alive.lock()) SetScrollOffset({horizontal_->Position(), vertical_->Position()});
  });
  vertical_->SetPositionChangedCallback([this, alive](std::int32_t) {
    if (alive.lock()) SetScrollOffset({horizontal_->Position(), vertical_->Position()});
  });
}

void UiPanelScrollBar::SetHorizontalActive(bool value) {
  horizontal_->SetActive(value);
  if (value) horizontal_->UpdateSizeForOrientation();
  UpdateScrollbarPlacement();
}
void UiPanelScrollBar::SetVerticalActive(bool value) {
  vertical_->SetActive(value);
  if (value) vertical_->UpdateSizeForOrientation();
  UpdateScrollbarPlacement();
}
void UiPanelScrollBar::MoveBarsToExpectedParent() {
  auto* desired = external_ ? Parent() : static_cast<UiObject*>(this);
  if (!desired) return;
  for (auto* bar : {horizontal_, vertical_}) {
    if (bar->Parent() == desired) continue;
    if (bar->Reparent(desired)) bar->SetDepth(external_ ? Depth() : -1.0e30);
  }
}
void UiPanelScrollBar::FinalizeAfterAttach() {
  MoveBarsToExpectedParent();
  UpdateScrollbarPlacement();
}
void UiPanelScrollBar::SetExternal(bool value) {
  if (external_ == value) return;
  external_ = value;
  MoveBarsToExpectedParent();
  UpdateScrollbarPlacement();
}

bool UiPanelScrollBar::UpdateScrollbarPlacement(std::string* error) {
  const auto size = ClientSize();
  if (size.width < 0 || size.height < 0 || size.width > 16384 || size.height > 16384)
    return Fail(error, "PanelScrollBar geometry exceeds safe bounds");
  const auto safe_rect = [](Rect rect) {
    return rect.right >= rect.left && rect.bottom >= rect.top &&
        static_cast<std::int64_t>(rect.right) - rect.left <= 16384 &&
        static_cast<std::int64_t>(rect.bottom) - rect.top <= 16384;
  };
  if ((!auto_horizontal_ && !safe_rect(horizontal_rect_)) ||
      (!auto_vertical_ && !safe_rect(vertical_rect_)))
    return Fail(error, "PanelScrollBar manual rectangle exceeds safe bounds");
  const auto horizontal_height = horizontal_ ? horizontal_->ClientSize().height : 0;
  const auto vertical_width = vertical_ ? vertical_->ClientSize().width : 0;
  if (!horizontal_ || !vertical_) return true;
  if (auto_horizontal_) {
    horizontal_->UiObject::SetPosition(external_ ?
        Point{LocalPosition().x, Bound(static_cast<std::int64_t>(LocalPosition().y) + size.height)} :
        Point{0, size.height - horizontal_height});
    horizontal_->SetSize({std::max(0, size.width - (external_ ? 0 : vertical_width)),
                          horizontal_height});
  } else {
    horizontal_->UiObject::SetPosition({horizontal_rect_.left, horizontal_rect_.top});
    horizontal_->SetSize({horizontal_rect_.right - horizontal_rect_.left,
                          horizontal_rect_.bottom - horizontal_rect_.top});
  }
  if (auto_vertical_) {
    vertical_->UiObject::SetPosition(external_ ?
        Point{Bound(static_cast<std::int64_t>(LocalPosition().x) + size.width), LocalPosition().y} :
        Point{size.width - vertical_width, 0});
    vertical_->SetSize({vertical_width,
                        std::max(0, size.height - (external_ ? 0 : horizontal_height))});
  } else {
    vertical_->UiObject::SetPosition({vertical_rect_.left, vertical_rect_.top});
    vertical_->SetSize({vertical_rect_.right - vertical_rect_.left,
                        vertical_rect_.bottom - vertical_rect_.top});
  }
  if (error) error->clear();
  return true;
}

bool UiPanelScrollBar::UpdateScrollRanges(std::string* error) {
  if (!Active()) return true;
  bool found{};
  Rect bounds{};
  for (const auto& child : Children()) {
    if (child.get() == horizontal_ || child.get() == vertical_ ||
        !child->PositionModeW() || !child->Active()) continue;
    const auto current = child->LocalBounds();
    if (!found) { bounds = current; found = true; }
    else bounds = Union(bounds, current);
  }
  if (!found) bounds = {0, 0, std::max(1, ClientSize().width),
                         std::max(1, ClientSize().height)};
  horizontal_->SetRange(bounds.left, Bound(static_cast<std::int64_t>(bounds.right) - 1));
  horizontal_->SetPageSize(ClientSize().width);
  horizontal_->SetPositionInternal(ScrollOffset().x);
  vertical_->SetRange(bounds.top, Bound(static_cast<std::int64_t>(bounds.bottom) - 1));
  vertical_->SetPageSize(ClientSize().height);
  vertical_->SetPositionInternal(ScrollOffset().y);
  if (!unlimited_world_)
    UiPanel::SetScrollOffset({horizontal_->Position(), vertical_->Position()});
  if (error) error->clear();
  return true;
}

void UiPanelScrollBar::SetScrollOffset(Point value) {
  UiPanel::SetScrollOffset(value);
  horizontal_->SetPositionInternal(value.x);
  vertical_->SetPositionInternal(value.y);
  if (!unlimited_world_)
    UiPanel::SetScrollOffset({horizontal_->Position(), vertical_->Position()});
}

Point UiPanelScrollBar::ChildAbsolutePosition(const UiObject& child) const {
  if (&child == horizontal_ || &child == vertical_)
    return UiObject::ChildAbsolutePosition(child);
  return UiPanel::ChildAbsolutePosition(child);
}

}  // namespace srhd_awa::platform::ui

#include "ui_object.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace srhd_awa::platform::ui {
UiObject::~UiObject() {
  if (mutation_observer_) mutation_observer_(this);
}
void UiObject::PropagateObserver(const MutationObserver& observer) {
  mutation_observer_ = observer;
  for (auto& child : children_) child->PropagateObserver(observer);
}
void UiObject::SetMutationObserver(MutationObserver observer) { PropagateObserver(observer); }
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
}  // namespace

Rect Intersect(Rect left, Rect right) {
  return {std::max(left.left, right.left), std::max(left.top, right.top),
          std::min(left.right, right.right), std::min(left.bottom, right.bottom)};
}

Rect Union(Rect left, Rect right) {
  if (IsEmpty(left)) return right;
  if (IsEmpty(right)) return left;
  return {std::min(left.left, right.left), std::min(left.top, right.top),
          std::max(left.right, right.right), std::max(left.bottom, right.bottom)};
}

UiObject* UiObject::Root() {
  UiObject* value = this;
  while (value->parent_) value = value->parent_;
  return value;
}
const UiObject* UiObject::Root() const {
  const UiObject* value = this;
  while (value->parent_) value = value->parent_;
  return value;
}

bool UiObject::IsAncestorOf(const UiObject* candidate) const {
  for (auto* current = candidate; current; current = current->parent_) {
    if (current == this) return true;
  }
  return false;
}

void UiObject::InsertOwned(std::unique_ptr<UiObject> child) {
  child->parent_ = this;
  child->PropagateObserver(mutation_observer_);
  // Upstream inserts before the first sibling whose depth is <= new depth.
  const auto position = std::find_if(children_.begin(), children_.end(), [&](const auto& existing) {
    return existing->depth_ <= child->depth_;
  });
  children_.insert(position, std::move(child));
}

bool UiObject::Attach(std::unique_ptr<UiObject> child, std::string* error) {
  if (!child) return Fail(error, "UI child is null");
  if (child->parent_) return Fail(error, "UI child already has a parent");
  if (child.get() == this || child->IsAncestorOf(this)) return Fail(error, "UI attach would create a cycle");
  InsertOwned(std::move(child));
  RefreshActiveSubtree();
  return true;
}

std::unique_ptr<UiObject> UiObject::Detach(UiObject* child) {
  const auto it = std::find_if(children_.begin(), children_.end(), [&](const auto& item) { return item.get() == child; });
  if (it == children_.end()) return {};
  if (mutation_observer_) mutation_observer_(child);
  auto result = std::move(*it);
  children_.erase(it);
  result->parent_ = nullptr;
  result->PropagateObserver({});
  result->RefreshActiveSubtree();
  return result;
}

bool UiObject::Reparent(UiObject* new_parent, std::string* error) {
  if (!new_parent) return Fail(error, "UI reparent parent is null");
  if (!parent_) return Fail(error, "UI root cannot be reparented");
  if (new_parent == this || IsAncestorOf(new_parent)) return Fail(error, "UI reparent would create a cycle");
  auto owned = parent_->Detach(this);
  if (!owned) return Fail(error, "UI reparent source is detached");
  if (!new_parent->Attach(std::move(owned), error)) return false;
  return true;
}

UiObject* UiObject::AddObject() {
  auto child = std::make_unique<UiObject>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

UiPanel* UiObject::AddPanel() {
  auto child = std::make_unique<UiPanel>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

void UiObject::SetPosition(Point value) {
  MarkSubtreeDirty();
  local_position_ = value;
  RefreshActiveSubtree();
  OnGeometryChanged();
  MarkSubtreeDirty();
}
void UiObject::SetSize(Size value) {
  MarkSubtreeDirty();
  client_size_ = value;
  RefreshActiveSubtree();
  OnGeometryChanged();
  MarkSubtreeDirty();
}
void UiObject::SetOrigin(Point value) {
  MarkSubtreeDirty();
  origin_ = value;
  RefreshActiveSubtree();
  OnGeometryChanged();
  MarkSubtreeDirty();
}
void UiObject::SetDepth(double value) {
  if (depth_ == value) return;
  depth_ = value;
  ReinsertInParent();
}
void UiObject::SetPositionModeW(bool value) {
  if (position_mode_w_ == value) return;
  MarkSubtreeDirty();
  position_mode_w_ = value;
  RefreshActiveSubtree();
  MarkSubtreeDirty();
}
void UiObject::SetActive(bool value) {
  if (active_ == value) return;
  if (!value && mutation_observer_) mutation_observer_(this);
  MarkSubtreeDirty();
  active_ = value;
  if (active_) RefreshActiveSubtree();
  MarkSubtreeDirty();
}

Rect UiObject::LocalBounds() const {
  return {local_position_.x - origin_.x, local_position_.y - origin_.y,
          local_position_.x - origin_.x + client_size_.width,
          local_position_.y - origin_.y + client_size_.height};
}
bool UiObject::ContainsPoint(Point point) const {
  return active_ && !hit_test_disabled_ && Contains(hit_test_bounds_, point);
}
Point UiObject::ToLocalPoint(Point point) const {
  return {point.x - absolute_position_.x, point.y - absolute_position_.y};
}
Point UiObject::ToAbsolutePoint(Point point) const {
  return {point.x + absolute_position_.x, point.y + absolute_position_.y};
}
UiObject* UiObject::FindByNameRecursive(const std::string& name) {
  if (name_ == name) return this;
  for (const auto& child : children_) {
    if (auto* result = child->FindByNameRecursive(name)) return result;
  }
  return nullptr;
}
const UiObject* UiObject::FindByNameRecursive(const std::string& name) const {
  if (name_ == name) return this;
  for (const auto& child : children_) {
    if (const auto* result = child->FindByNameRecursive(name)) return result;
  }
  return nullptr;
}

Point UiObject::ChildAbsolutePosition(const UiObject& child) const {
  return {absolute_position_.x + child.local_position_.x, absolute_position_.y + child.local_position_.y};
}
void UiObject::RefreshActiveSubtree() {
  const Point absolute = parent_ ? parent_->ChildAbsolutePosition(*this) : local_position_;
  RefreshActiveSubtreeFrom(absolute);
}
void UiObject::RefreshActiveSubtreeFrom(Point absolute) {
  absolute_position_ = absolute;
  hit_test_bounds_ = {absolute.x - origin_.x, absolute.y - origin_.y,
                      absolute.x - origin_.x + client_size_.width, absolute.y - origin_.y + client_size_.height};
  OnAbsoluteGeometryChanged();
  for (const auto& child : children_) {
    child->RefreshActiveSubtreeFrom(ChildAbsolutePosition(*child));
  }
}
void UiObject::AddDirty(Rect rect) {
  if (!IsEmpty(rect)) Root()->dirty_rects_.push_back(rect);
}
void UiObject::Invalidate() { if (active_) AddDirty(hit_test_bounds_); }
void UiObject::MarkSubtreeDirty() {
  if (!active_) return;
  Invalidate();
  for (const auto& child : children_) child->MarkSubtreeDirty();
}
const std::vector<Rect>& UiObject::DirtyRects() const { return Root()->dirty_rects_; }
void UiObject::ClearDirtyRects() { Root()->dirty_rects_.clear(); }
void UiObject::ReinsertInParent() {
  if (!parent_) return;
  UiObject* const previous_parent = parent_;
  auto owned = previous_parent->Detach(this);
  if (owned) previous_parent->InsertOwned(std::move(owned));
  Root()->RefreshActiveSubtree();
}

Point UiPanel::ChildAbsolutePosition(const UiObject& child) const {
  Point value = UiObject::ChildAbsolutePosition(child);
  if (child.PositionModeW()) {
    value.x -= scroll_offset_.x;
    value.y -= scroll_offset_.y;
  }
  return value;
}
void UiPanel::SetScrollOffset(Point value) {
  if (value == scroll_offset_) return;
  MarkSubtreeDirty();
  scroll_offset_ = value;
  RefreshActiveSubtree();
  MarkSubtreeDirty();
}
Rect UiPanel::GetVisibleContentRect() const {
  const Point origin = Origin();
  const Size size = ClientSize();
  return {scroll_offset_.x - origin.x, scroll_offset_.y - origin.y,
          scroll_offset_.x - origin.x + size.width, scroll_offset_.y - origin.y + size.height};
}
void UiPanel::ScrollRectIntoView(Rect rect) {
  const Rect visible = GetVisibleContentRect();
  Point offset = scroll_offset_;
  if (rect.bottom > visible.bottom) offset.y += rect.bottom - visible.bottom;
  if (rect.top < visible.top) offset.y += rect.top - visible.top;
  if (rect.right > visible.right) offset.x += rect.right - visible.right;
  if (rect.left < visible.left) offset.x += rect.left - visible.left;
  SetScrollOffset(offset);
}
Point UiPanel::ToLocalPoint(Point point) const {
  const Point absolute = AbsolutePosition();
  return {point.x - absolute.x + scroll_offset_.x, point.y - absolute.y + scroll_offset_.y};
}
Point UiPanel::ToAbsolutePoint(Point point) const {
  const Point absolute = AbsolutePosition();
  return {point.x + absolute.x - scroll_offset_.x, point.y + absolute.y - scroll_offset_.y};
}

UiTree::UiTree() : root_(std::make_unique<UiPanel>()) { root_->SetName("root"); }

}  // namespace srhd_awa::platform::ui

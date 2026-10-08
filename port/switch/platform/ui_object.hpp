#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace srhd_awa::platform::scene_compositor { struct Framebuffer; }

namespace srhd_awa::platform::ui {

struct Point {
  std::int32_t x{};
  std::int32_t y{};
  friend constexpr bool operator==(Point, Point) = default;
};

struct Size {
  std::int32_t width{};
  std::int32_t height{};
  friend constexpr bool operator==(Size, Size) = default;
};

// Portable UI rectangles use half-open edges: [left,right) x [top,bottom).
struct Rect {
  std::int32_t left{};
  std::int32_t top{};
  std::int32_t right{};
  std::int32_t bottom{};
  friend constexpr bool operator==(Rect, Rect) = default;
};

constexpr bool IsEmpty(Rect value) {
  return value.right <= value.left || value.bottom <= value.top;
}
constexpr bool Contains(Rect value, Point point) {
  return point.x >= value.left && point.x < value.right && point.y >= value.top && point.y < value.bottom;
}
Rect Intersect(Rect left, Rect right);
Rect Union(Rect left, Rect right);

enum class NodeKind { Object, Panel, ImageLeaf, GILeaf };
enum class ScrollType { Simple, All, Obj, View };

class UiObject {
 public:
  UiObject() = default;
  virtual ~UiObject() = default;
  UiObject(const UiObject&) = delete;
  UiObject& operator=(const UiObject&) = delete;
  UiObject(UiObject&&) = delete;
  UiObject& operator=(UiObject&&) = delete;

  virtual NodeKind Kind() const { return NodeKind::Object; }
  UiObject* Parent() const { return parent_; }
  const std::vector<std::unique_ptr<UiObject>>& Children() const { return children_; }
  std::size_t ChildCount() const { return children_.size(); }

  bool Attach(std::unique_ptr<UiObject> child, std::string* error = nullptr);
  std::unique_ptr<UiObject> Detach(UiObject* child);
  bool Reparent(UiObject* new_parent, std::string* error = nullptr);

  UiObject* AddObject();
  class UiPanel* AddPanel();

  void SetPosition(Point value);
  void SetSize(Size value);
  void SetOrigin(Point value);
  void SetDepth(double value);
  void SetPositionModeW(bool value);
  void SetActive(bool value);
  void SetHitTestDisabled(bool value) { hit_test_disabled_ = value; }
  void SetName(std::string value) { name_ = std::move(value); }

  Point LocalPosition() const { return local_position_; }
  Point AbsolutePosition() const { return absolute_position_; }
  Size ClientSize() const { return client_size_; }
  Point Origin() const { return origin_; }
  double Depth() const { return depth_; }
  bool PositionModeW() const { return position_mode_w_; }
  bool Active() const { return active_; }
  bool HitTestDisabled() const { return hit_test_disabled_; }
  const std::string& Name() const { return name_; }
  Rect LocalBounds() const;
  Rect HitTestBounds() const { return hit_test_bounds_; }
  bool ContainsPoint(Point point) const;
  virtual Point ToLocalPoint(Point point) const;
  virtual Point ToAbsolutePoint(Point point) const;
  UiObject* FindByNameRecursive(const std::string& name);
  const UiObject* FindByNameRecursive(const std::string& name) const;
  void UpdateGeometry() { RefreshActiveSubtree(); }

  // The root owns accumulated full-frame invalidation evidence. It is only a
  // geometry model: M22 deliberately does not implement upstream queued redraw.
  void Invalidate();
  const std::vector<Rect>& DirtyRects() const;
  void ClearDirtyRects();

 protected:
  virtual Point ChildAbsolutePosition(const UiObject& child) const;
  void RefreshActiveSubtree();
  void RefreshActiveSubtreeFrom(Point absolute);
  void MarkSubtreeDirty();

 private:
  friend class UiPanel;
  UiObject* Root();
  const UiObject* Root() const;
  bool IsAncestorOf(const UiObject* candidate) const;
  void InsertOwned(std::unique_ptr<UiObject> child);
  void ReinsertInParent();
  void AddDirty(Rect rect);

  UiObject* parent_{};
  std::vector<std::unique_ptr<UiObject>> children_;
  Point local_position_{};
  Point absolute_position_{};
  Size client_size_{};
  Point origin_{};
  Rect hit_test_bounds_{};
  double depth_{};
  bool position_mode_w_{};
  bool active_{true};
  bool hit_test_disabled_{};
  std::string name_;
  std::vector<Rect> dirty_rects_;
};

class UiPanel final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::Panel; }
  void SetScrollOffset(Point value);
  Point ScrollOffset() const { return scroll_offset_; }
  void SetScrollType(ScrollType value) { scroll_type_ = value; }
  ScrollType GetScrollType() const { return scroll_type_; }
  Rect GetVisibleContentRect() const;
  void ScrollRectIntoView(Rect rect);
  Point ToLocalPoint(Point point) const override;
  Point ToAbsolutePoint(Point point) const override;

 protected:
  Point ChildAbsolutePosition(const UiObject& child) const override;

 private:
  Point scroll_offset_{};
  ScrollType scroll_type_{ScrollType::Simple};
};

class UiTree {
 public:
  UiTree();
  UiPanel* Root() { return root_.get(); }
  const UiPanel* Root() const { return root_.get(); }
  void SetRootSize(Size size) { root_->SetSize(size); }
  void UpdateGeometry() { root_->UpdateGeometry(); }
  bool Update(std::uint64_t delta_ms, std::string* error = nullptr);
  bool Render(const scene_compositor::Framebuffer& target, std::string* error = nullptr) const;
  const std::vector<Rect>& DirtyRects() const { return root_->DirtyRects(); }
  void ClearDirtyRects() { root_->ClearDirtyRects(); }

 private:
  std::unique_ptr<UiPanel> root_;
};

}  // namespace srhd_awa::platform::ui

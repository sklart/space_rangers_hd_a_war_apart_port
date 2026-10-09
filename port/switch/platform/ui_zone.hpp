#pragma once

#include "ui_object.hpp"

namespace srhd_awa::platform::ui {

enum class ZoneKind { Rect, Circle };

// Circle mode deliberately retains the upstream active/disabled quirk, while
// Rect delegates to the M22 half-open ContainsPoint path. The M27 router owns
// dispatch and calls this shape-specific hit test separately from tree bounds.
class UiZone final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::Zone; }
  void SetZoneKind(ZoneKind value) { zone_kind_ = value; }
  ZoneKind GetZoneKind() const { return zone_kind_; }
  bool HitTest(Point point) const;
  bool CursorInside() const { return cursor_inside_; }
  void SetCursorInside(bool value) { cursor_inside_ = value; }

 private:
  ZoneKind zone_kind_{ZoneKind::Rect};
  bool cursor_inside_{};
};

}  // namespace srhd_awa::platform::ui

#pragma once

#include "ui_object.hpp"

namespace srhd_awa::platform::ui {

enum class ZoneKind { Rect, Circle };

// Pure geometry: event callbacks and the mouse dispatcher belong to a later
// milestone.  Circle mode deliberately retains the upstream active/disabled
// quirk, while Rect delegates to the M22 half-open ContainsPoint path.
class UiZone final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::Zone; }
  void SetZoneKind(ZoneKind value) { zone_kind_ = value; }
  ZoneKind GetZoneKind() const { return zone_kind_; }
  bool HitTest(Point point) const;

 private:
  ZoneKind zone_kind_{ZoneKind::Rect};
};

}  // namespace srhd_awa::platform::ui

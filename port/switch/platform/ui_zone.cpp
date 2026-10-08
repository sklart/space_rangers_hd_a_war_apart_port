#include "ui_zone.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace srhd_awa::platform::ui {

UiZone* UiObject::AddZone() {
  auto child = std::make_unique<UiZone>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

bool UiZone::HitTest(Point point) const {
  if (zone_kind_ == ZoneKind::Rect) return ContainsPoint(point);
  const Rect bounds = HitTestBounds();
  const auto width = std::int64_t(bounds.right) - bounds.left;
  const auto height = std::int64_t(bounds.bottom) - bounds.top;
  const auto diameter = std::min(width, height);
  if (diameter <= 0) return false;
  const auto center_x = (std::int64_t(bounds.left) + bounds.right) / 2;
  const auto center_y = (std::int64_t(bounds.top) + bounds.bottom) / 2;
  const auto dx = std::int64_t(point.x) - center_x;
  const auto dy = std::int64_t(point.y) - center_y;
  // Multiply by four instead of dividing the diameter so odd diameters keep
  // the same half-pixel radius as the upstream floating-point comparison.
  return 4.0L * (static_cast<long double>(dx) * dx +
                 static_cast<long double>(dy) * dy) <=
         static_cast<long double>(diameter) * diameter;
}

}  // namespace srhd_awa::platform::ui

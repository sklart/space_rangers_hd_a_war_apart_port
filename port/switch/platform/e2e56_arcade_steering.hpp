#pragma once
#include <algorithm>
#include <cmath>

namespace srhd_awa::platform::e2e56_arcade_steering {
struct Command { double turn{}; bool thrust{}; };
inline Command Steer(double projected_heading, int x, int y) {
  // Screen headings: up=0, right=90. HID positive Y points up.
  if ((!x && !y) || !std::isfinite(projected_heading)) return {};
  const double desired = std::atan2(static_cast<double>(x), static_cast<double>(y)) *
      (180.0 / 3.14159265358979323846);
  const double delta = std::remainder(desired - projected_heading, 360.0);
  // Use the existing turn-rate limit and thrust physics. When pointing away
  // from the requested direction, turn first rather than accelerating away.
  return {std::clamp(delta, -100.0, 100.0), std::abs(delta) < 60.0};
}
}

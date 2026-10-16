#pragma once
#include <cstdint>
#include <cstdlib>

namespace srhd_awa::platform {
// UI-thread state only. Delay a map tap until release, so a swipe never orders a ship.
struct MapSwipe {
  const void* owner{};
  int x{}, y{}, center_x{}, center_y{};
  std::uint32_t keys{};
  bool dragging{};
  void Cancel() { *this = {}; }
  void Begin(const void* screen, int px, int py, int cx, int cy, std::uint32_t state) {
    *this = {screen, px, py, cx, cy, state, false};
  }
  bool Move(const void* screen, int px, int py) {
    if (owner != screen) return false;
    if (std::abs(px - x) >= 12 || std::abs(py - y) >= 12) dragging = true;
    return dragging;
  }
};
}

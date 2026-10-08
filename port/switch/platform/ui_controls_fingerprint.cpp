#include "ui_controls_fingerprint.hpp"

#include "ui_graph_button.hpp"
#include "ui_label.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_window.hpp"
#include "ui_zone.hpp"

#include <cstdint>
#include <vector>

namespace srhd_awa::platform::ui_controls_fingerprint {
namespace {
void U8(std::vector<std::uint8_t>* bytes, std::uint8_t value) { bytes->push_back(value); }
void U16(std::vector<std::uint8_t>* bytes, std::uint16_t value) {
  U8(bytes, value); U8(bytes, value >> 8);
}
void I32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
  const auto bits = static_cast<std::uint32_t>(value);
  for (unsigned i{}; i < 4; ++i) U8(bytes, bits >> (i * 8));
}
void Point(std::vector<std::uint8_t>* bytes, ui::Point point) {
  I32(bytes, point.x); I32(bytes, point.y);
}
void Size(std::vector<std::uint8_t>* bytes, ui::Size size) {
  I32(bytes, size.width); I32(bytes, size.height);
}
bool Finish(const std::vector<std::uint8_t>& bytes, Value* result) {
  if (!result) return false;
  std::uint32_t crc = 0xffffffffu;
  std::uint64_t fnv = 0xcbf29ce484222325ull;
  for (const auto byte : bytes) {
    crc ^= byte;
    for (unsigned bit{}; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    fnv = (fnv ^ byte) * 0x100000001b3ull;
  }
  result->crc32 = crc ^ 0xffffffffu;
  result->fnv64 = fnv;
  result->bytes = bytes.size();
  return true;
}
}  // namespace

bool ComputeWindowLayout(const ui::UiWindow& window, Value* result) {
  std::vector<std::uint8_t> bytes;
  Size(&bytes, window.MinimumSize());
  const auto work = window.WorkSubRect();
  I32(&bytes, work.left); I32(&bytes, work.top); I32(&bytes, work.right); I32(&bytes, work.bottom);
  for (unsigned i{}; i < static_cast<unsigned>(ui::WindowSlot::Count); ++i) {
    const auto* image = window.BorderImage(static_cast<ui::WindowSlot>(i));
    if (!image) return false;
    Point(&bytes, image->LocalPosition());
    Size(&bytes, image->ClientSize());
    U8(&bytes, image->Active());
    U8(&bytes, static_cast<std::uint8_t>(image->Image().x_mode()));
    U8(&bytes, static_cast<std::uint8_t>(image->Image().y_mode()));
  }
  return Finish(bytes, result);
}

bool ComputeGraphState(const ui::UiGraphButton& button, Value* result) {
  std::vector<std::uint8_t> bytes;
  U8(&bytes, static_cast<std::uint8_t>(button.ButtonKind()));
  U8(&bytes, static_cast<std::uint8_t>(button.HitKind()));
  U8(&bytes, button.Hovered()); U8(&bytes, button.Down());
  U8(&bytes, button.Disabled()); U8(&bytes, button.UpOnlyDown());
  U8(&bytes, static_cast<std::uint8_t>(button.VisualSlot()));
  const auto* caption = button.Caption();
  U8(&bytes, caption != nullptr);
  Point(&bytes, caption ? caption->LocalPosition() : ui::Point{});
  U16(&bytes, caption ? caption->TextColor() : 0);
  I32(&bytes, caption ? caption->TextShadowOffset() : 0);
  U16(&bytes, caption ? caption->TextShadowColor() : 0);
  for (unsigned i{}; i < static_cast<unsigned>(ui::GraphButtonSlot::Count); ++i) {
    const auto slot = static_cast<ui::GraphButtonSlot>(i);
    const auto* image = button.StateImage(slot);
    U8(&bytes, image != nullptr);
    Point(&bytes, button.StateOffset(slot));
    U8(&bytes, image && image->Active());
  }
  return Finish(bytes, result);
}

bool ComputeZoneHits(const ui::UiZone& zone, const std::vector<ui::Point>& points,
                     Value* result) {
  std::vector<std::uint8_t> bytes;
  U8(&bytes, static_cast<std::uint8_t>(zone.GetZoneKind()));
  I32(&bytes, static_cast<std::int32_t>(points.size()));
  for (const auto point : points) {
    Point(&bytes, point);
    U8(&bytes, zone.HitTest(point));
  }
  return Finish(bytes, result);
}

}  // namespace srhd_awa::platform::ui_controls_fingerprint

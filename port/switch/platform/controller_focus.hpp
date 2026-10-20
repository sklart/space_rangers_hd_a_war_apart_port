#pragma once
#include <cstdint>
#include <limits>
#include <vector>

namespace srhd_awa::platform::controller_focus {
enum class Direction { Up, Down, Left, Right };
struct Node {
  std::uint64_t id{};
  std::int32_t x{}, y{}, width{}, height{};
  bool enabled{true};
};
inline bool Enabled(const Node& node) {
  return node.id && node.enabled && node.width > 0 && node.height > 0;
}
inline const Node* Find(const std::vector<Node>& nodes, std::uint64_t id) {
  for (const auto& node : nodes) if (node.id == id && Enabled(node)) return &node;
  return nullptr;
}
inline std::uint64_t First(const std::vector<Node>& nodes) {
  for (const auto& node : nodes) if (Enabled(node)) return node.id;
  return 0;
}
inline std::uint64_t Navigate(const std::vector<Node>& nodes, std::uint64_t current,
                              Direction direction) {
  const auto* origin = Find(nodes, current);
  if (!origin) return First(nodes);
  const auto cx = std::int64_t(origin->x) * 2 + origin->width;
  const auto cy = std::int64_t(origin->y) * 2 + origin->height;
  const bool horizontal = direction == Direction::Left || direction == Direction::Right;
  const int sign = direction == Direction::Left || direction == Direction::Up ? -1 : 1;
  auto score_best = std::numeric_limits<std::int64_t>::max();
  std::uint64_t result = current;
  for (const auto& node : nodes) {
    if (!Enabled(node) || node.id == current) continue;
    const auto dx = std::int64_t(node.x) * 2 + node.width - cx;
    const auto dy = std::int64_t(node.y) * 2 + node.height - cy;
    const auto along = (horizontal ? dx : dy) * sign;
    const auto across = horizontal ? (dy < 0 ? -dy : dy) : (dx < 0 ? -dx : dx);
    if (along <= 0) continue;
    const auto score = along + across * 4;
    if (score < score_best) { score_best = score; result = node.id; }
  }
  if (result != current) return result;
  // Lists/grids wrap to the opposite edge, preferring the same row/column.
  score_best = std::numeric_limits<std::int64_t>::max();
  for (const auto& node : nodes) {
    if (!Enabled(node) || node.id == current) continue;
    const auto dx = std::int64_t(node.x) * 2 + node.width - cx;
    const auto dy = std::int64_t(node.y) * 2 + node.height - cy;
    const auto along = (horizontal ? dx : dy) * sign;
    const auto across = horizontal ? (dy < 0 ? -dy : dy) : (dx < 0 ? -dx : dx);
    if (along >= 0) continue;
    const auto score = across * 8 + along;
    if (score < score_best) { score_best = score; result = node.id; }
  }
  return result;
}
} // namespace srhd_awa::platform::controller_focus

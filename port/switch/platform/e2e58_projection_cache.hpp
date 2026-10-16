#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <utility>

namespace srhd_awa::platform {
// One immutable matrix and floating-point mode per update. Only the result of
// the original projector is reused; no arithmetic or projection order changes.
template<class Vector, class Projector> class ProjectionCache {
  struct Entry {
    std::array<std::uint64_t, 3> key{};
    Vector value{};
    bool valid{};
  };
  std::array<Entry, 128> entries_{};
  Projector projector_;
  std::uint64_t hits_{}, misses_{};
public:
  explicit ProjectionCache(Projector projector) : projector_(std::move(projector)) {}
  std::uint64_t Hits() const noexcept { return hits_; }
  std::uint64_t Misses() const noexcept { return misses_; }
  Vector Project(const Vector& point) {
    const std::array key{std::bit_cast<std::uint64_t>(point.X),
                         std::bit_cast<std::uint64_t>(point.Y),
                         std::bit_cast<std::uint64_t>(point.Z)};
    auto hash = key[0] ^ (key[1] * UINT64_C(0x9e3779b97f4a7c15)) ^ key[2];
    hash ^= hash >> 33;
    hash *= UINT64_C(0xff51afd7ed558ccd);
    hash ^= hash >> 33;
    auto& entry = entries_[hash & (entries_.size() - 1)];
    if (entry.valid && entry.key == key) { ++hits_; return entry.value; }
    // Publish only after a successful projection, including exceptional paths.
    const auto value = projector_(point);
    ++misses_;
    entry = {key, value, true};
    return value;
  }
};
template<class Vector, class Projector>
auto MakeProjectionCache(Projector projector) {
  return ProjectionCache<Vector, Projector>(std::move(projector));
}
}

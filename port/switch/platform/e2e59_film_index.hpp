#pragma once
#include <array>
#include <cstdint>
#include <utility>

namespace srhd_awa::platform {
// Valid only while serializing one immutable object list. Misses retain the
// original lookup and exception; no unknown object is assigned an ordinal.
template<class Pointer, class Lookup> class FilmIndex {
  struct Entry { Pointer object{}; std::int32_t ordinal{}; bool valid{}; };
  std::array<Entry, 128> entries_{};
  Lookup lookup_;
public:
  explicit FilmIndex(Lookup lookup) : lookup_(std::move(lookup)) {}
  std::int32_t Find(Pointer object) {
    auto hash = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object)) >> 4;
    hash ^= hash >> 17;
    hash *= UINT64_C(0x9e3779b97f4a7c15);
    hash ^= hash >> 33;
    auto& entry = entries_[hash & (entries_.size() - 1)];
    if (entry.valid && entry.object == object) return entry.ordinal;
    const auto ordinal = lookup_(object);
    entry = {object, ordinal, true};
    return ordinal;
  }
};
template<class Pointer, class Lookup> auto MakeFilmIndex(Lookup lookup) {
  return FilmIndex<Pointer, Lookup>(std::move(lookup));
}
}

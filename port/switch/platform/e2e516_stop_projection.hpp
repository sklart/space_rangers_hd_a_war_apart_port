#pragma once
#include <array>
#include <cstdint>
namespace srhd_awa::platform {
// One query, one immutable matrix and immutable stop points. No cross-tick cache.
template<class Point, class Vector, class Project>
class StopProjectionMemo {
  struct Entry { const Point* point{}; Vector projected{}; };
  std::array<Entry,64> entries_{};
  Project project_;
 public:
  explicit StopProjectionMemo(Project project):project_(project){}
  Vector Get(const Point* point) {
    auto& entry=entries_[(reinterpret_cast<std::uintptr_t>(point)>>4)%entries_.size()];
    if(entry.point==point)return entry.projected;
    auto projected=project_(point);
    entry={point,projected};return projected;
  }
};
}

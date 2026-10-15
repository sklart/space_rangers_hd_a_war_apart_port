#pragma once

#include "e2e_stage.hpp"
#include "units/EC_Cache.hpp"
#include "units/GR_Main.hpp"
#include <cstdio>
#if defined(__SWITCH__)
#include <malloc.h>
#include <unistd.h>
extern "C" char* fake_heap_start;
extern "C" char* fake_heap_end;
#endif

namespace srhd_awa::platform::e2e5_memory_recovery {
// Only unretained software cache entries can be reclaimed. In particular, the
// entry currently being loaded is retained and cannot be evicted here.
inline bool Reclaim(const char* operation, std::int32_t requested) noexcept {
  static thread_local bool active = false;
  if (active || !GR_Main::GlobalCache) return false;
  active = true;
  bool reclaimed = false;
  try {
    const auto before = GR_Main::GlobalCache->ResidentBytes;
    GR_Main::GlobalCache->TrimToBudget(0);
    const auto after = GR_Main::GlobalCache->ResidentBytes;
    reclaimed = after < before;
    char detail[320]{};
#if defined(__SWITCH__)
    const auto stats = mallinfo();
    const auto end = reinterpret_cast<std::uintptr_t>(fake_heap_end);
    const auto brk = reinterpret_cast<std::uintptr_t>(sbrk(0));
    std::snprintf(detail, sizeof(detail),
        "allocation recovery operation=%s bytes=%d cache_before=%d cache_after=%d arena=%u used=%u free_blocks=%u top_free=%u heap_uncommitted=%llu",
        operation, requested, before, after,
        static_cast<unsigned>(stats.arena), static_cast<unsigned>(stats.uordblks),
        static_cast<unsigned>(stats.fordblks), static_cast<unsigned>(stats.keepcost),
        static_cast<unsigned long long>(brk <= end ? end - brk : 0));
#else
    std::snprintf(detail, sizeof(detail),
        "allocation recovery operation=%s bytes=%d cache_before=%d cache_after=%d",
        operation, requested, before, after);
#endif
    e2e_stage::Log(detail);
  } catch (...) {
    // Retain the original allocation failure even if reclamation cannot finish.
  }
  active = false;
  return reclaimed;
}
} // namespace srhd_awa::platform::e2e5_memory_recovery

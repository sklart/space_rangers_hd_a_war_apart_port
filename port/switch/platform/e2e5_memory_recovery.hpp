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
  auto* cache = GR_Main::GlobalCache;
  if (active || !cache) return false;
  active = true;
  bool reclaimed = false;
  try {
    std::int32_t before = 0, after = 0;
    {
      // The cache lock is recursive. Keep both snapshots and trimming in the
      // same transaction as concurrent loader publication/eviction.
      pas::critical_enter(cache->CacheLock);
      pas::ScopeExit unlock = [&]() noexcept { pas::critical_leave(cache->CacheLock); };
      before = cache->ResidentBytes;
      cache->TrimToBudget(0);
      after = cache->ResidentBytes;
    }
    // Diagnostic I/O must not extend the cache critical section.
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

#pragma once
#include "e2e_stage.hpp"
#if defined(__SWITCH__)
#include <malloc.h>
#include <unistd.h>
extern "C" char* fake_heap_end;
#endif
namespace srhd_awa::platform::e2e5_failure_diagnostics {
// Avoid Log's std::string stage assignment while handling allocation failures.
inline void Write(const char* text) noexcept {
  try {
    std::lock_guard lock(e2e_stage::g_stage_mutex);
    e2e_stage::Append("[E2E] ", text);
  } catch (...) {}
}
inline void Memory(const char* operation) noexcept {
#if defined(__SWITCH__)
  const auto stats = mallinfo();
  const auto end = reinterpret_cast<std::uintptr_t>(fake_heap_end);
  const auto brk = reinterpret_cast<std::uintptr_t>(sbrk(0));
  char text[256]{};
  std::snprintf(text, sizeof(text),
      "memory operation=%s arena=%u used=%u free_blocks=%u top_free=%u uncommitted=%llu",
      operation, static_cast<unsigned>(stats.arena), static_cast<unsigned>(stats.uordblks),
      static_cast<unsigned>(stats.fordblks), static_cast<unsigned>(stats.keepcost),
      static_cast<unsigned long long>(brk <= end ? end - brk : 0));
  Write(text);
#else
  (void)operation;
#endif
}
} // namespace srhd_awa::platform::e2e5_failure_diagnostics

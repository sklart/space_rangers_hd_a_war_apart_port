#pragma once

#include <cstdint>
#if defined(__SWITCH__)
#include <switch.h>
#include "e2e_stage.hpp"
#endif

namespace srhd_awa::platform::e2e_threads {

using Entry = std::int32_t (*)(void*);

std::uint32_t Create(Entry entry, void* parameter, bool suspended,
                     std::uint32_t* thread_id);
std::uint32_t Resume(std::uint32_t handle);
std::uint32_t Wait(std::uint32_t handle, std::uint32_t timeout_ms);
bool Close(std::uint32_t handle);
bool SetPriority(std::uint32_t handle, std::int32_t priority);
bool PreferCalculationCore(std::uint32_t handle);
std::int32_t GetPriority(std::uint32_t handle);
std::uint32_t CurrentId();

#if defined(__SWITCH__)
inline void LogPriority(const char* role, Handle handle, std::int32_t requested,
                        Result set_result) {
  std::int32_t actual = -1;
  const Result query = svcGetThreadPriority(&actual, handle);
  char detail[192]{};
  std::snprintf(detail, sizeof(detail),
      "thread priority role=%s handle=%u requested=%d actual=%d set_result=%u query_result=%u",
      role, handle, requested, actual, set_result, query);
  e2e_stage::Log(detail);
}
#endif

}  // namespace srhd_awa::platform::e2e_threads

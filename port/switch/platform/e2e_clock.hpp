#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::e2e_clock {
inline std::uint64_t Counter() {
#if defined(__SWITCH__)
  return armGetSystemTick();
#else
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}

inline std::uint64_t Frequency() {
#if defined(__SWITCH__)
  return armGetSystemTickFreq();
#else
  return 1000000000ULL;
#endif
}

inline std::uint32_t Milliseconds() {
#if defined(__SWITCH__)
  return static_cast<std::uint32_t>(armTicksToNs(Counter()) / 1000000ULL);
#else
  return static_cast<std::uint32_t>(Counter() / 1000000ULL);
#endif
}

inline void SleepMilliseconds(std::uint32_t milliseconds) {
#if defined(__SWITCH__)
  svcSleepThread(static_cast<std::int64_t>(milliseconds) * 1000000LL);
#else
  if (milliseconds == 0) std::this_thread::yield();
  else std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}
}  // namespace srhd_awa::platform::e2e_clock

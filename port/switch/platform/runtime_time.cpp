#if defined(__SWITCH__)
#include <switch.h>
#else
#include <chrono>
#endif

#include "units/WindowsImports.hpp"

// Narrow replacement for the two timing imports reached by
// SystemImports::Randomize. It deliberately does not emulate the Win32 API.
namespace WindowsImports {
std::int32_t PAS_STDCALL QueryPerformanceCounter(std::int64_t& counter) {
#if defined(__SWITCH__)
  counter = static_cast<std::int64_t>(armGetSystemTick());
#else
  counter = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
#endif
  return 1;
}

std::uint32_t PAS_STDCALL GetTickCount() {
#if defined(__SWITCH__)
  return static_cast<std::uint32_t>(armTicksToNs(armGetSystemTick()) / 1000000ULL);
#else
  return static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
#endif
}
}  // namespace WindowsImports

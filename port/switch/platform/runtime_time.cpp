#include <switch.h>

#include "units/WindowsImports.hpp"

// Narrow replacement for the two timing imports reached by
// SystemImports::Randomize. It deliberately does not emulate the Win32 API.
namespace WindowsImports {
std::int32_t PAS_STDCALL QueryPerformanceCounter(std::int64_t& counter) {
  counter = static_cast<std::int64_t>(armGetSystemTick());
  return 1;
}

std::uint32_t PAS_STDCALL GetTickCount() {
  return static_cast<std::uint32_t>(armTicksToNs(armGetSystemTick()) / 1000000ULL);
}
}  // namespace WindowsImports

#include "e2e_clock.hpp"
#include "units/WindowsImports.hpp"
#include "units/WindowsSdk.hpp"
#include "units/SysUtilsImports.hpp"

// Portable clock and sleep entry points used by the diagnostic runtime.
// The complete-game build injects the same clock behind the original APIs.
namespace WindowsImports {
std::int32_t PAS_STDCALL QueryPerformanceCounter(std::int64_t& counter) {
  counter = static_cast<std::int64_t>(srhd_awa::platform::e2e_clock::Counter());
  return 1;
}

std::uint32_t PAS_STDCALL GetTickCount() {
  return srhd_awa::platform::e2e_clock::Milliseconds();
}
}  // namespace WindowsImports

namespace WindowsSdk {
BOOL PAS_STDCALL QueryPerformanceFrequency(Windows::TLargeInteger& frequency) {
  frequency = static_cast<Windows::TLargeInteger>(srhd_awa::platform::e2e_clock::Frequency());
  return frequency > 0;
}
}  // namespace WindowsSdk

namespace SysUtilsImports {
void PAS_STDCALL Sleep(std::uint32_t milliseconds) {
  srhd_awa::platform::e2e_clock::SleepMilliseconds(milliseconds);
}
}  // namespace SysUtilsImports

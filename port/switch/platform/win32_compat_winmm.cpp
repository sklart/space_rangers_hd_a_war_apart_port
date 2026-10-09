#include "win32_compat_winmm.hpp"
#include "e2e_clock.hpp"
#include "win32_handles.hpp"
#include <cstdint>

namespace srhd_awa::platform::win32_compat {
namespace {
std::uint32_t TimeGetTime() { return e2e_clock::Milliseconds(); }
std::uint32_t TimeBeginPeriod(std::uint32_t period) {
  if (!period) { SetLastError(kErrorInvalidParameter); return 1; }
  SetLastError(kErrorSuccess);
  return 0; // Switch owns the host timer resolution.
}
std::uint32_t TimeEndPeriod(std::uint32_t period) {
  return TimeBeginPeriod(period);
}
std::uint32_t TimeSetEvent(std::uint32_t, std::uint32_t, void*,
                           std::uint32_t, std::uint32_t) {
  SetLastError(kErrorAccessDenied);
  return 0; // Audio timers are unavailable in E2E-1.
}
std::uint32_t TimeKillEvent(std::uint32_t) {
  SetLastError(kErrorInvalidHandle);
  return 1;
}
template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}
ImportAddress ResolveWinmmImport(std::string_view dll, std::string_view symbol) {
  if (dll != "winmm.dll") return nullptr;
  if (symbol == "timeGetTime") return Address(&TimeGetTime);
  if (symbol == "timeBeginPeriod") return Address(&TimeBeginPeriod);
  if (symbol == "timeEndPeriod") return Address(&TimeEndPeriod);
  if (symbol == "timeSetEvent") return Address(&TimeSetEvent);
  if (symbol == "timeKillEvent") return Address(&TimeKillEvent);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat

#include "win32_compat_filetime.hpp"

#include "e2e_calendar.hpp"
#include "win32_handles.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>

namespace srhd_awa::platform::win32_compat {
namespace {
constexpr std::uint64_t kEpoch100ns = 116444736000000000ull;
constexpr std::uint64_t kTicksPerMillisecond = 10000ull;
std::mutex g_time_mutex;

std::uint64_t Ticks(const Windows::TFileTime* input) {
  return (static_cast<std::uint64_t>(input->dwHighDateTime) << 32) |
         input->dwLowDateTime;
}
Windows::TFileTime FromTicks(std::uint64_t ticks) {
  return {static_cast<std::uint32_t>(ticks),
          static_cast<std::uint32_t>(ticks >> 32)};
}
std::int64_t Milliseconds(const Windows::TFileTime* input) {
  const auto ticks = Ticks(input);
  if (ticks >= kEpoch100ns)
    return static_cast<std::int64_t>((ticks - kEpoch100ns) / kTicksPerMillisecond);
  return -static_cast<std::int64_t>((kEpoch100ns - ticks) / kTicksPerMillisecond);
}

std::int32_t Compare(const Windows::TFileTime* first,
                     const Windows::TFileTime* second) {
  if (!first || !second) { SetLastError(kErrorInvalidParameter); return 0; }
  const auto a = Ticks(first), b = Ticks(second);
  SetLastError(kErrorSuccess);
  return a < b ? -1 : a > b ? 1 : 0;
}

std::int32_t ToSystem(const Windows::TFileTime* input,
                      Windows::TSystemTime* output) {
  if (!input || !output) { SetLastError(kErrorInvalidParameter); return 0; }
  e2e_calendar::UtcFromUnixMilliseconds(Milliseconds(input), *output);
  SetLastError(output->wYear ? kErrorSuccess : kErrorInvalidParameter);
  return output->wYear ? 1 : 0;
}

std::int32_t ToLocal(const Windows::TFileTime* input,
                     Windows::TFileTime* output) {
  if (!input || !output) { SetLastError(kErrorInvalidParameter); return 0; }
  const auto utc_ms = Milliseconds(input);
  const auto seconds = static_cast<std::time_t>(utc_ms / 1000);
  std::int64_t offset_seconds = 0;
  {
    std::lock_guard lock(g_time_mutex);
    const auto* local_ptr = std::localtime(&seconds);
    if (!local_ptr) { SetLastError(kErrorInvalidParameter); return 0; }
    const auto local = *local_ptr;
    const auto* utc_ptr = std::gmtime(&seconds);
    if (!utc_ptr) { SetLastError(kErrorInvalidParameter); return 0; }
    auto utc = *utc_ptr;
    auto local_copy = local;
    offset_seconds = static_cast<std::int64_t>(std::mktime(&utc) -
                                               std::mktime(&local_copy));
  }
  *output = UnixMillisToFileTime(utc_ms - offset_seconds * 1000);
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t ToDos(const Windows::TFileTime* input,
                   std::uint16_t* date, std::uint16_t* time) {
  if (!input || !date || !time) { SetLastError(kErrorInvalidParameter); return 0; }
  Windows::TSystemTime calendar{};
  if (!ToSystem(input, &calendar) || calendar.wYear < 1980 || calendar.wYear > 2107) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  *date = static_cast<std::uint16_t>(((calendar.wYear - 1980) << 9) |
                                    (calendar.wMonth << 5) | calendar.wDay);
  *time = static_cast<std::uint16_t>((calendar.wHour << 11) |
                                    (calendar.wMinute << 5) | (calendar.wSecond / 2));
  SetLastError(kErrorSuccess);
  return 1;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

Windows::TFileTime UnixMillisToFileTime(std::int64_t milliseconds) {
  const auto ticks = static_cast<std::int64_t>(kEpoch100ns) +
                     milliseconds * static_cast<std::int64_t>(kTicksPerMillisecond);
  return FromTicks(static_cast<std::uint64_t>(ticks));
}

ImportAddress ResolveFileTimeImport(std::string_view dll, std::string_view symbol) {
  if (dll != "kernel32.dll") return nullptr;
  if (symbol == "CompareFileTime") return Address(&Compare);
  if (symbol == "FileTimeToSystemTime") return Address(&ToSystem);
  if (symbol == "FileTimeToLocalFileTime") return Address(&ToLocal);
  if (symbol == "FileTimeToDosDateTime") return Address(&ToDos);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat

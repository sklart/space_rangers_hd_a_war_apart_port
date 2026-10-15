#include "win32_compat_filetime.hpp"
#include "win32_handles.hpp"

#include <cassert>
#include <cstdint>

using namespace srhd_awa::platform::win32_compat;

int main() {
  // 2024-02-29 12:34:56 UTC.
  const auto value = UnixMillisToFileTime(1709210096000ll);
  const auto earlier = UnixMillisToFileTime(0);
  using Compare = std::int32_t (*)(const Windows::TFileTime*, const Windows::TFileTime*);
  using Calendar = std::int32_t (*)(const Windows::TFileTime*, Windows::TSystemTime*);
  using Dos = std::int32_t (*)(const Windows::TFileTime*, std::uint16_t*, std::uint16_t*);
  const auto compare = reinterpret_cast<Compare>(ResolveFileTimeImport("kernel32.dll", "CompareFileTime"));
  const auto calendar = reinterpret_cast<Calendar>(ResolveFileTimeImport("kernel32.dll", "FileTimeToSystemTime"));
  const auto dos = reinterpret_cast<Dos>(ResolveFileTimeImport("kernel32.dll", "FileTimeToDosDateTime"));
  assert(compare(&value, &earlier) == 1 && compare(&value, &value) == 0);
  Windows::TSystemTime decoded{};
  assert(calendar(&value, &decoded));
  assert(decoded.wYear == 2024 && decoded.wMonth == 2 && decoded.wDay == 29);
  assert(decoded.wHour == 12 && decoded.wMinute == 34 && decoded.wSecond == 56);
  std::uint16_t date = 0, time = 0;
  assert(dos(&value, &date, &time));
  assert(((date >> 9) + 1980) == 2024 && ((date >> 5) & 15) == 2 && (date & 31) == 29);
  assert((time >> 11) == 12 && ((time >> 5) & 63) == 34 && (time & 31) == 28);
  assert(!calendar(nullptr, &decoded) && GetLastError() == kErrorInvalidParameter);
}

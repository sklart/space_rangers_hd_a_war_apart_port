#include "e2e_calendar.hpp"

#include <chrono>
#include <ctime>
#include <mutex>

namespace srhd_awa::platform::e2e_calendar {
namespace {
std::mutex g_calendar_mutex;

void Fill(std::time_t seconds, std::uint16_t milliseconds, bool local,
          Windows::TSystemTime& output) {
  std::lock_guard lock(g_calendar_mutex);
  const std::tm* calendar = local ? std::localtime(&seconds) : std::gmtime(&seconds);
  if (!calendar) {
    output = {};
    return;
  }
  output.wYear = static_cast<std::uint16_t>(calendar->tm_year + 1900);
  output.wMonth = static_cast<std::uint16_t>(calendar->tm_mon + 1);
  output.wDayOfWeek = static_cast<std::uint16_t>(calendar->tm_wday);
  output.wDay = static_cast<std::uint16_t>(calendar->tm_mday);
  output.wHour = static_cast<std::uint16_t>(calendar->tm_hour);
  output.wMinute = static_cast<std::uint16_t>(calendar->tm_min);
  output.wSecond = static_cast<std::uint16_t>(calendar->tm_sec);
  output.wMilliseconds = milliseconds;
}
}  // namespace

void UtcFromUnixMilliseconds(std::int64_t milliseconds,
                             Windows::TSystemTime& output) {
  auto seconds = milliseconds / 1000;
  auto remainder = milliseconds % 1000;
  if (remainder < 0) {
    remainder += 1000;
    --seconds;
  }
  Fill(static_cast<std::time_t>(seconds), static_cast<std::uint16_t>(remainder),
       false, output);
}

void GetSystemTime(Windows::TSystemTime& output) {
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  UtcFromUnixMilliseconds(milliseconds, output);
}

void GetLocalTime(Windows::TSystemTime& output) {
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  auto seconds = milliseconds / 1000;
  auto remainder = milliseconds % 1000;
  if (remainder < 0) {
    remainder += 1000;
    --seconds;
  }
  Fill(static_cast<std::time_t>(seconds), static_cast<std::uint16_t>(remainder),
       true, output);
}
}  // namespace srhd_awa::platform::e2e_calendar

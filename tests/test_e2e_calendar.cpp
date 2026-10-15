#include "e2e_calendar.hpp"

#include <cassert>

int main() {
  Windows::TSystemTime utc{};
  srhd_awa::platform::e2e_calendar::UtcFromUnixMilliseconds(
      1582979696789LL, utc);
  assert(utc.wYear == 2020 && utc.wMonth == 2 && utc.wDay == 29);
  assert(utc.wHour == 12 && utc.wMinute == 34 && utc.wSecond == 56);
  assert(utc.wMilliseconds == 789 && utc.wDayOfWeek == 6);
  srhd_awa::platform::e2e_calendar::GetSystemTime(utc);
  assert(utc.wYear >= 2026 && utc.wMonth >= 1 && utc.wMonth <= 12);
  assert(utc.wMilliseconds < 1000);
}

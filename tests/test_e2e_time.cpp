#include "units/SysUtilsImports.hpp"
#include "units/WindowsImports.hpp"
#include "units/WindowsSdk.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>

int main() {
  Windows::TLargeInteger frequency = 0;
  std::int64_t before = 0;
  std::int64_t after = 0;
  if (!WindowsSdk::QueryPerformanceFrequency(frequency) || frequency <= 0 ||
      !WindowsImports::QueryPerformanceCounter(before)) return 1;
  const auto host_start = std::chrono::steady_clock::now();
  const auto tick_before = WindowsImports::GetTickCount();
  SysUtilsImports::Sleep(40);
  const auto tick_after = WindowsImports::GetTickCount();
  const auto host_elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - host_start).count();
  if (!WindowsImports::QueryPerformanceCounter(after)) return 2;
  const double counter_elapsed = static_cast<double>(after - before) /
      static_cast<double>(frequency);
  if (host_elapsed < 0.035 || host_elapsed > 1.0 ||
      counter_elapsed < 0.035 || counter_elapsed > 1.0 ||
      (tick_after - tick_before) < 35 || (tick_after - tick_before) > 1000) {
    std::fprintf(stderr, "time mismatch: host=%f qpc=%f tick_ms=%u\n",
                 host_elapsed, counter_elapsed, tick_after - tick_before);
    return 3;
  }
  SysUtilsImports::Sleep(0);
  std::printf("E2E time PASS: host=%f qpc=%f tick_ms=%u frequency=%lld\n",
              host_elapsed, counter_elapsed, tick_after - tick_before,
              static_cast<long long>(frequency));
}

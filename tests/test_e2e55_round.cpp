#include "e2e5_round.hpp"
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
#include <type_traits>
namespace fast = srhd_awa::platform::e2e5_round;
static_assert(std::is_same_v<decltype(fast::Round(0.5L)), std::int64_t>);
template<class T> void Check(T value) {
  assert(fast::Round(value) == System::Round(static_cast<pas::Extended>(value)));
}
int main() {
  std::uint64_t state = 0x3141592653589793ULL;
  for (unsigned mode=0; mode<4; ++mode) {
    pas::set_fpu_control(static_cast<std::uint16_t>(0x133f | (mode << 10)));
    for (double value : {0., -0., .5, -.5, 1.5, 2.5, -1.5, -2.5,
                        0x1p63, -0x1p63, std::nextafter(0x1p63, 0.),
                        std::numeric_limits<double>::denorm_min(),
                        -std::numeric_limits<double>::denorm_min(),
                        std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()}) Check(value);
    for (unsigned i=0; i<500000; ++i) {
      state ^= state << 13; state ^= state >> 7; state ^= state << 17;
      Check(std::bit_cast<double>(state));
      Check(std::bit_cast<float>(static_cast<std::uint32_t>(state)));
    }
  }
  pas::set_fpu_control(0x133e); // Invalid-operation errors enabled: preserve fallback exceptions.
  for (double value : {0x1p63, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
    bool old_error=false,new_error=false;
    try { (void)System::Round(value); } catch (const pas::Raised&) {old_error=true;}
    try { (void)fast::Round(value); } catch (const pas::Raised&) {new_error=true;}
    assert(old_error && new_error);
  }
  pas::set_fpu_control(0x133f);
  volatile std::uint64_t checksum = 0;
  for (bool optimized : {false, true}) {
    checksum = 0;
    auto begin = std::chrono::steady_clock::now();
    for (unsigned i=0; i<250000; ++i) {
      const double value = (static_cast<int>(i) - 125000) * .125;
      checksum = checksum + (optimized ? fast::Round(value) : System::Round(value));
    }
    std::printf("round %s us=%lld checksum=%llu\n", optimized ? "native" : "reference",
      static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-begin).count()), static_cast<unsigned long long>(checksum));
  }
  std::puts("4 rounding modes, 4 million binary32/64 patterns and boundaries PASS");
}

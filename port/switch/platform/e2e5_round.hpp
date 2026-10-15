#pragma once
#include "units/System.hpp"
#include <concepts>
#include <cmath>
#include <cstdint>

namespace srhd_awa::platform::e2e5_round {
// Only values already stored as IEEE binary32/64 take this path. Do not
// narrow Extended expressions: their intermediate precision is game logic.
inline std::int64_t Round(pas::Extended value) { return System::Round(value); }
template<class T> requires (std::same_as<T, float> || std::same_as<T, double>)
inline std::int64_t Round(T value) {
#if defined(__aarch64__)
  const double input = value;
  if (std::isfinite(input) && input >= -0x1p63 && input < 0x1p63) {
    double rounded;
    switch ((pas::get_fpu_control() >> 10) & 3) {
      case 1: asm("frintm %d0, %d1" : "=w"(rounded) : "w"(input)); break;
      case 2: asm("frintp %d0, %d1" : "=w"(rounded) : "w"(input)); break;
      case 3: asm("frintz %d0, %d1" : "=w"(rounded) : "w"(input)); break;
      default: asm("frintn %d0, %d1" : "=w"(rounded) : "w"(input)); break;
    }
    return static_cast<std::int64_t>(rounded);
  }
#endif
  return System::Round(static_cast<pas::Extended>(value));
}
} // namespace srhd_awa::platform::e2e5_round

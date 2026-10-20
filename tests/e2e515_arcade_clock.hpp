#pragma once
#include <cstdint>
// Diagnostic UI-thread clock. Shipping sources never include this header.
namespace e2e515_arcade_clock {
inline thread_local bool enabled{};
inline thread_local std::uint32_t milliseconds{};
}

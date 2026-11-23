// Test-only accounting for real EC_Buf owners and controlled pre-queue errors.
#pragma once
#include <atomic>
namespace matrixgame_save_buffer_probe {
inline std::atomic<long> live{};
inline thread_local int fail_before{};
inline void Created() noexcept { ++live; }
inline void Destroyed() noexcept { --live; }
}

#pragma once

#include <cstdint>

namespace srhd_awa::platform::e2e_events {

inline constexpr std::uint32_t kWaitObject0 = 0;
inline constexpr std::uint32_t kWaitTimeout = 258;
inline constexpr std::uint32_t kWaitFailed = 0xffffffffu;
inline constexpr std::uint32_t kInfinite = 0xffffffffu;

std::uint32_t Create(bool manual_reset, bool initial_state);
bool Close(std::uint32_t handle);
bool Set(std::uint32_t handle);
bool Reset(std::uint32_t handle);
std::uint32_t WaitOne(std::uint32_t handle, std::uint32_t timeout_ms);
std::uint32_t WaitMany(const std::uint32_t* handles, std::uint32_t count,
                       bool wait_all, std::uint32_t timeout_ms);

}  // namespace srhd_awa::platform::e2e_events

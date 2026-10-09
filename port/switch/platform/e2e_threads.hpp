#pragma once

#include <cstdint>

namespace srhd_awa::platform::e2e_threads {

using Entry = std::int32_t (*)(void*);

std::uint32_t Create(Entry entry, void* parameter, bool suspended,
                     std::uint32_t* thread_id);
std::uint32_t Resume(std::uint32_t handle);
std::uint32_t Wait(std::uint32_t handle, std::uint32_t timeout_ms);
bool Close(std::uint32_t handle);
bool SetPriority(std::uint32_t handle, std::int32_t priority);
std::int32_t GetPriority(std::uint32_t handle);
std::uint32_t CurrentId();

}  // namespace srhd_awa::platform::e2e_threads

#pragma once

#include <cstdint>

namespace srhd_awa::platform::e2e5_gai_slots {

// GI_GAIFile allocates FrameCount * sizeof(void*) bytes for these entries.
// The translated 32-bit ReadDWordEC/WriteIntegerEC path lost the high half.
inline void*& At(void* frame_buffers, std::int32_t index) {
  return static_cast<void**>(frame_buffers)[index];
}

}  // namespace srhd_awa::platform::e2e5_gai_slots

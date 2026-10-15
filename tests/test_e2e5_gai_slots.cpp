#include "e2e5_gai_slots.hpp"

#include <array>
#include <cassert>

int main() {
  std::array<void*, 2> frame_buffers{};
  std::array<int, 2> frames{};
  srhd_awa::platform::e2e5_gai_slots::At(frame_buffers.data(), 0) = &frames[0];
  srhd_awa::platform::e2e5_gai_slots::At(frame_buffers.data(), 1) = &frames[1];
  assert(srhd_awa::platform::e2e5_gai_slots::At(frame_buffers.data(), 0) == &frames[0]);
  assert(srhd_awa::platform::e2e5_gai_slots::At(frame_buffers.data(), 1) == &frames[1]);
  srhd_awa::platform::e2e5_gai_slots::At(frame_buffers.data(), 0) = nullptr;
  assert(srhd_awa::platform::e2e5_gai_slots::At(frame_buffers.data(), 0) == nullptr);
}

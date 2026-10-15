#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace srhd_awa::platform::e2e5_gai_bounds {

inline const char* Validate(const void* input, std::size_t size,
                            bool allow_terminal_empty_frame = false) {
  if (!input || size < 48 || size > 256u * 1024u * 1024u) return "buffer size";
  const auto* bytes = static_cast<const std::uint8_t*>(input);
  auto i32 = [&](std::size_t at) {
    std::int32_t value{};
    std::memcpy(&value, bytes + at, sizeof(value));
    return value;
  };
  if (std::memcmp(bytes, "gai\0", 4) != 0 || i32(4) != 1) return "header";
  const auto frames = i32(24);
  if (frames <= 0 || frames > 100000 ||
      i32(16) <= i32(8) || i32(20) <= i32(12) ||
      static_cast<std::uint64_t>(48) + static_cast<std::uint64_t>(frames) * 8 > size)
    return "frame directory";
  for (std::int32_t frame = 0; frame < frames; ++frame) {
    const auto offset = i32(48 + static_cast<std::size_t>(frame) * 8);
    const auto length = i32(52 + static_cast<std::size_t>(frame) * 8);
    if (offset == 0 && length == 0) continue;
    if (offset < 0 || length <= 0 || static_cast<std::size_t>(offset) > size ||
        static_cast<std::size_t>(length) > size - static_cast<std::size_t>(offset))
      return "frame bounds";
  }
  const auto sequence_offset = i32(32);
  const auto sequence_size = i32(36);
  if (sequence_offset == 0 && sequence_size == 0) return nullptr;
  if (sequence_offset < 0 || sequence_size < 8 ||
      static_cast<std::size_t>(sequence_offset) > size ||
      static_cast<std::size_t>(sequence_size) > size - static_cast<std::size_t>(sequence_offset))
    return "sequence bounds";
  const auto base = static_cast<std::size_t>(sequence_offset);
  const auto sequences = i32(base);
  if (sequences < 0 || sequences > 100000 ||
      static_cast<std::uint64_t>(8) + static_cast<std::uint64_t>(sequences) * 8 >
          static_cast<std::uint32_t>(sequence_size))
    return "sequence directory";
  for (std::int32_t sequence = 0; sequence < sequences; ++sequence) {
    const auto block = i32(base + 8 + static_cast<std::size_t>(sequence) * 8);
    if (block < 0 || static_cast<std::uint64_t>(block) + 4 >
                         static_cast<std::uint32_t>(sequence_size))
      return "sequence block";
    const auto count = i32(base + static_cast<std::size_t>(block));
    if (count < 0 || count > 100000 ||
        static_cast<std::uint64_t>(block) + 4 + static_cast<std::uint64_t>(count) * 8 >
            static_cast<std::uint32_t>(sequence_size))
      return "sequence frames";
    for (std::int32_t frame = 0; frame < count; ++frame) {
      const auto source = i32(base + static_cast<std::size_t>(block) + 4 +
                              static_cast<std::size_t>(frame) * 8);
      if (source < 0 || source >= frames) {
        // Weapon/Effect/05.GAI has one final source index equal to FrameCount.
        // The software renderer treats that one-past index as a blank end frame.
        if (!(allow_terminal_empty_frame && sequence == sequences - 1 &&
              frame == count - 1 && source == frames))
          return "sequence frame index";
      }
    }
  }
  return nullptr;
}

}  // namespace srhd_awa::platform::e2e5_gai_bounds

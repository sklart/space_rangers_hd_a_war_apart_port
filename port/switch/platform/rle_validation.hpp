#pragma once

#include <cstddef>
#include <cstdint>

namespace srhd_awa::platform::rle_validation {

inline std::uint32_t Read32(const std::uint8_t* bytes) {
  return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
         (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

// OKGF uses 0 as an end-of-row marker and 0x80 as an empty-row marker.
// The latter may occur only before any pixels in that row.
inline bool ValidateStream(const std::uint8_t* stream, std::size_t stream_size,
                           std::int32_t width, std::int32_t height,
                           std::size_t literal_bytes) {
  if (!stream || width <= 0 || height <= 0 || literal_bytes > 4) return false;
  std::size_t at = 0;
  for (std::int32_t row = 0; row < height; ++row) {
    std::int32_t x = 0;
    for (;;) {
      if (at == stream_size) return false;
      const auto command = stream[at++];
      if (command == 0) { if (x != width) return false; break; }
      if (command == 0x80) { if (x != 0) return false; break; }
      const auto count = static_cast<std::int32_t>(command & 0x7f);
      if (count <= 0 || count > width - x) return false;
      if (command & 0x80) {
        const auto bytes = static_cast<std::size_t>(count) * literal_bytes;
        if (bytes > stream_size - at) return false;
        at += bytes;
      }
      x += count;
    }
  }
  return at == stream_size;
}

inline bool ValidateBuffer(const std::uint8_t* bytes, std::size_t size,
                           std::int32_t width, std::int32_t height,
                           std::size_t literal_bytes) {
  return bytes && size >= 16 && Read32(bytes) == size - 16 &&
         Read32(bytes + 4) == static_cast<std::uint32_t>(width) &&
         Read32(bytes + 8) == static_cast<std::uint32_t>(height) &&
         ValidateStream(bytes + 16, size - 16, width, height, literal_bytes);
}

}  // namespace srhd_awa::platform::rle_validation

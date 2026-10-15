#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace srhd_awa::platform::okgf_fast_alpha {

// Matches OKGF's 53-bit startup table truncation used by OKGR_PixelAlpha_16.
constexpr std::uint8_t Scale63(unsigned channel, unsigned factor) {
  const unsigned product = channel * factor;
  return static_cast<std::uint8_t>(
      (product - static_cast<unsigned>(factor > 32 && channel != 0)) / 63);
}

constexpr auto MakeScaleTable() {
  std::array<std::array<std::uint8_t, 64>, 64> table{};
  for (unsigned factor = 0; factor < 64; ++factor)
    for (unsigned channel = 0; channel < 64; ++channel)
      table[factor][channel] = Scale63(channel, factor);
  return table;
}

inline constexpr auto kScaleTable = MakeScaleTable();

inline void AlphaSimpleBufDraw16(void* dest, std::int32_t dest_pitch,
                                  std::int32_t dest_x, std::int32_t dest_y,
                                  void* source, std::int32_t source_pitch,
                                  std::int32_t source_x, std::int32_t source_y,
                                  std::int32_t width, std::int32_t height) {
  if (width <= 0 || height <= 0) return;
  auto* dest_row = static_cast<std::uint8_t*>(dest) +
      static_cast<std::ptrdiff_t>(dest_y) * dest_pitch + 2 * static_cast<std::ptrdiff_t>(dest_x);
  auto* source_row = static_cast<const std::uint8_t*>(source) +
      static_cast<std::ptrdiff_t>(source_y) * source_pitch + 4 * static_cast<std::ptrdiff_t>(source_x);
  for (std::int32_t y = 0; y < height; ++y) {
    auto* d = dest_row;
    auto* s = source_row;
    for (std::int32_t x = 0; x < width; ++x, d += 2, s += 4) {
      const unsigned alpha = s[3] >> 2;
      if (alpha == 0) continue;
      const std::uint16_t color = static_cast<std::uint16_t>(
          ((s[2] >> 3) << 11) | ((s[1] >> 2) << 5) | (s[0] >> 3));
      if (alpha >= 62) {
        std::memcpy(d, &color, sizeof(color));
        continue;
      }
      std::uint16_t old{};
      std::memcpy(&old, d, sizeof(old));
      const auto& source_scale = kScaleTable[alpha];
      const auto& dest_scale = kScaleTable[63 - alpha];
      const std::uint16_t blended = static_cast<std::uint16_t>(
          ((source_scale[color >> 11] + dest_scale[old >> 11]) << 11) |
          ((source_scale[(color >> 5) & 63] + dest_scale[(old >> 5) & 63]) << 5) |
          (source_scale[color & 31] + dest_scale[old & 31]));
      std::memcpy(d, &blended, sizeof(blended));
    }
    dest_row += dest_pitch;
    source_row += source_pitch;
  }
}

}  // namespace srhd_awa::platform::okgf_fast_alpha

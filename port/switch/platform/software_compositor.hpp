#pragma once

#include <cstdint>
#include <string>

namespace srhd_awa::platform::software_compositor {

struct Rect {
  std::int32_t left{};
  std::int32_t top{};
  std::int32_t right{};
  std::int32_t bottom{};
};

enum class BlendMode { Opaque, Alpha };

// Composites a BGRA8888 source into an RGB565 destination. Pitches are byte
// counts for src and pixel counts for dst. The module owns no renderer state.
bool CompositeBGRA(std::uint16_t* dst, std::int32_t dst_width,
                   std::int32_t dst_height, std::int32_t dst_pitch_pixels,
                   const std::uint8_t* src, std::int32_t src_width,
                   std::int32_t src_height, std::int32_t src_pitch,
                   std::int32_t dst_x, std::int32_t dst_y, BlendMode mode,
                   const Rect* clip, std::string* error = nullptr);

}  // namespace srhd_awa::platform::software_compositor

#include "software_compositor.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::software_compositor {
namespace {

bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}

bool CheckedEnd(std::int32_t start, std::int32_t extent, std::int64_t* end) {
  if (extent < 0) return false;
  *end = static_cast<std::int64_t>(start) + static_cast<std::int64_t>(extent);
  return *end >= std::numeric_limits<std::int32_t>::min() &&
         *end <= std::numeric_limits<std::int32_t>::max();
}

std::uint16_t Pack565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(red >> 3) << 11) |
                                    (static_cast<std::uint16_t>(green >> 2) << 5) |
                                    static_cast<std::uint16_t>(blue >> 3));
}

void Unpack565(std::uint16_t pixel, std::uint8_t* red, std::uint8_t* green,
               std::uint8_t* blue) {
  *red = static_cast<std::uint8_t>(((pixel >> 11) & 0x1f) * 255 / 31);
  *green = static_cast<std::uint8_t>(((pixel >> 5) & 0x3f) * 255 / 63);
  *blue = static_cast<std::uint8_t>((pixel & 0x1f) * 255 / 31);
}

std::uint8_t Blend(std::uint8_t source, std::uint8_t destination,
                   std::uint8_t alpha) {
  return static_cast<std::uint8_t>((static_cast<std::uint32_t>(source) * alpha +
                                    static_cast<std::uint32_t>(destination) *
                                        (255u - alpha) +
                                    127u) /
                                   255u);
}

}  // namespace

bool CompositeBGRA(std::uint16_t* dst, std::int32_t dst_width,
                   std::int32_t dst_height, std::int32_t dst_pitch_pixels,
                   const std::uint8_t* src, std::int32_t src_width,
                   std::int32_t src_height, std::int32_t src_pitch,
                   std::int32_t dst_x, std::int32_t dst_y, BlendMode mode,
                   const Rect* clip, std::string* error,
                   std::uint8_t global_alpha) {
  if (!dst || !src) return Fail(error, "null compositor buffer");
  if (dst_width <= 0 || dst_height <= 0 || dst_pitch_pixels < dst_width ||
      src_width <= 0 || src_height <= 0 ||
      static_cast<std::int64_t>(src_pitch) <
          static_cast<std::int64_t>(src_width) * 4)
    return Fail(error, "invalid compositor dimensions or pitch");
  if (static_cast<std::uint64_t>(dst_pitch_pixels) *
          static_cast<std::uint64_t>(dst_height) >
          static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ||
      static_cast<std::uint64_t>(src_pitch) * static_cast<std::uint64_t>(src_height) >
          static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
    return Fail(error, "compositor buffer range too large");

  std::int64_t source_right{};
  std::int64_t source_bottom{};
  if (!CheckedEnd(dst_x, src_width, &source_right) ||
      !CheckedEnd(dst_y, src_height, &source_bottom))
    return Fail(error, "compositor coordinates overflow");

  std::int64_t left = std::max<std::int64_t>(dst_x, 0);
  std::int64_t top = std::max<std::int64_t>(dst_y, 0);
  std::int64_t right = std::min<std::int64_t>(source_right, dst_width);
  std::int64_t bottom = std::min<std::int64_t>(source_bottom, dst_height);
  if (clip) {
    if (clip->right < clip->left || clip->bottom < clip->top)
      return Fail(error, "invalid clip rectangle");
    left = std::max<std::int64_t>(left, clip->left);
    top = std::max<std::int64_t>(top, clip->top);
    right = std::min<std::int64_t>(right, clip->right);
    bottom = std::min<std::int64_t>(bottom, clip->bottom);
  }
  if (left >= right || top >= bottom) return true;

  for (std::int64_t y = top; y < bottom; ++y) {
    const auto source_y = y - dst_y;
    const auto* source_row = src + source_y * src_pitch;
    auto* destination_row = dst + y * dst_pitch_pixels;
    for (std::int64_t x = left; x < right; ++x) {
      const auto source_x = x - dst_x;
      const auto* pixel = source_row + source_x * 4;
      const std::uint8_t blue = pixel[0];
      const std::uint8_t green = pixel[1];
      const std::uint8_t red = pixel[2];
      const std::uint8_t alpha = static_cast<std::uint8_t>(
          (static_cast<std::uint16_t>(pixel[3]) * global_alpha + 127u) / 255u);
      if (mode == BlendMode::Opaque) {
        destination_row[x] = Pack565(red, green, blue);
      } else if (alpha != 0) {
        std::uint8_t destination_red{};
        std::uint8_t destination_green{};
        std::uint8_t destination_blue{};
        Unpack565(destination_row[x], &destination_red, &destination_green,
                  &destination_blue);
        destination_row[x] = Pack565(Blend(red, destination_red, alpha),
                                     Blend(green, destination_green, alpha),
                                     Blend(blue, destination_blue, alpha));
      }
    }
  }
  return true;
}

}  // namespace srhd_awa::platform::software_compositor

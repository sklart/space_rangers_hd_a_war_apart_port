#include "gi_image_cpu.hpp"

#include "gi_format2_cpu.hpp"
#include "package.hpp"
#include "software_compositor.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace srhd_awa::platform::gi_image_cpu {
namespace {
constexpr std::size_t kMaxSource = 256ull * 1024 * 1024;
bool Fail(std::string* error, const char* reason) { if (error) *error = reason; return false; }
std::int32_t I32(const std::uint8_t* p) {
  const auto value = static_cast<std::uint32_t>(p[0]) |
                     (static_cast<std::uint32_t>(p[1]) << 8) |
                     (static_cast<std::uint32_t>(p[2]) << 16) |
                     (static_cast<std::uint32_t>(p[3]) << 24);
  return static_cast<std::int32_t>(value);
}
bool Nonzero565(const std::uint8_t* pixel, std::uint8_t alpha) {
  const auto effective = (static_cast<unsigned>(pixel[3]) * alpha + 127u) / 255u;
  if (!effective) return false;
  // The hit oracle renders on black, then inspects the RGB565 result.
  const auto red = (static_cast<unsigned>(pixel[2]) * effective + 127u) / 255u;
  const auto green = (static_cast<unsigned>(pixel[1]) * effective + 127u) / 255u;
  const auto blue = (static_cast<unsigned>(pixel[0]) * effective + 127u) / 255u;
  return (red >> 3) || (green >> 2) || (blue >> 3);
}
}  // namespace

bool GiImage::Load(package::Package& package, const std::string& resource, std::string* error) {
  image_.Clear(); origin_x_ = origin_y_ = format_ = 0;
  const auto* entry = package.Resolve(resource);
  if (!entry) return Fail(error, "raw GI resource is missing");
  if (entry->data_size > kMaxSource) return Fail(error, "raw GI source exceeds limit");
  std::vector<std::uint8_t> source;
  if (!package.ReadPayload(*entry, &source, error)) return false;
  return LoadBytes(source.data(), source.size(), error);
}

bool GiImage::LoadBytes(const std::uint8_t* bytes, std::size_t size, std::string* error) {
  image_.Clear(); origin_x_ = origin_y_ = format_ = 0;
  if (!bytes || size < 64 || size > kMaxSource || bytes[0] != 'g' || bytes[1] != 'i')
    return Fail(error, "raw GI header or source size is invalid");
  if (I32(bytes + 4) != 1) return Fail(error, "raw GI version is unsupported");
  const auto width = static_cast<std::int64_t>(I32(bytes + 16)) - I32(bytes + 8);
  const auto height = static_cast<std::int64_t>(I32(bytes + 20)) - I32(bytes + 12);
  if (width <= 0 || height <= 0 || width > std::numeric_limits<std::int32_t>::max() ||
      height > std::numeric_limits<std::int32_t>::max() ||
      width * height > static_cast<std::int64_t>(kMaxSource / 4))
    return Fail(error, "raw GI dimensions exceed limit");
  const auto plane_count = I32(bytes + 44), clip_count = I32(bytes + 48);
  const auto clip_offset = I32(bytes + 52);
  if (plane_count < 1 || clip_count < 0 ||
      static_cast<std::uint64_t>(plane_count) * 32 > size - 64 ||
      (clip_count && (clip_offset < 0 ||
                      static_cast<std::uint64_t>(clip_offset) > size ||
                      static_cast<std::uint64_t>(clip_count) * 8 > size - clip_offset)))
    return Fail(error, "raw GI table bounds are invalid");
  for (std::int32_t index = 0; index < plane_count; ++index) {
    const auto offset = I32(bytes + 64 + static_cast<std::size_t>(index) * 32);
    const auto plane_size = I32(bytes + 68 + static_cast<std::size_t>(index) * 32);
    if (offset < 0 || plane_size < 0 ||
        (offset && (static_cast<std::uint64_t>(offset) > size ||
                    static_cast<std::uint64_t>(plane_size) > size - offset)))
      return Fail(error, "raw GI plane range is invalid");
  }
  const auto format = I32(bytes + 40);
  if (format == 0) {
    gi_format0_cpu::Metadata metadata{};
    if (gi_format0_cpu::Decode(bytes, size, &metadata, &image_, error) != gi_format0_cpu::Status::Ok)
      return false;
    origin_x_ = metadata.left; origin_y_ = metadata.top;
  } else if (format == 2) {
    gi_format2_cpu::Metadata metadata{};
    if (gi_format2_cpu::Decode(bytes, size, &metadata, &image_, error) != gi_format2_cpu::Status::Ok)
      return false;
    origin_x_ = metadata.left; origin_y_ = metadata.top;
  } else return Fail(error, "raw GI format is unsupported");
  format_ = format;
  return true;
}

bool GiImage::Draw(std::uint16_t* pixels, std::int32_t width, std::int32_t height,
                   std::int32_t pitch, okgf_rle_bridge::Rect bounds,
                   okgf_rle_bridge::Rect clip, image_layout::XMode x_mode,
                   image_layout::YMode y_mode, std::uint8_t alpha,
                   std::string* error) const {
  if (!loaded()) return Fail(error, "raw GI is not loaded");
  if (!pixels || width <= 0 || height <= 0 || pitch < width)
    return Fail(error, "raw GI framebuffer is invalid");
  if (clip.left < 0 || clip.top < 0 || clip.right > width || clip.bottom > height ||
      clip.right < clip.left || clip.bottom < clip.top)
    return Fail(error, "raw GI clip is invalid");
  const auto plan = image_layout::MakePlan(bounds, clip, image_.width, image_.height, x_mode, y_mode);
  if (!plan.supported) return true;
  const software_compositor::Rect compositor_clip{clip.left, clip.top, clip.right, clip.bottom};
  for (const auto& tile : plan.tiles)
    if (!software_compositor::CompositeBGRA(pixels, width, height, pitch, image_.pixels.data(),
                                            image_.width, image_.height, image_.pitch,
                                            tile.x, tile.y, software_compositor::BlendMode::Alpha,
                                            &compositor_clip, error, alpha)) return false;
  return true;
}

bool GiImage::HitTestPixel(std::int32_t x, std::int32_t y, okgf_rle_bridge::Rect bounds,
                           okgf_rle_bridge::Rect clip, image_layout::XMode x_mode,
                           image_layout::YMode y_mode, std::uint8_t alpha) const {
  if (!loaded() || !alpha || x < bounds.left || y < bounds.top || x >= bounds.right ||
      y >= bounds.bottom || x < clip.left || y < clip.top || x >= clip.right || y >= clip.bottom)
    return false;
  const auto plan = image_layout::MakePlan(bounds, clip, image_.width, image_.height, x_mode, y_mode);
  for (const auto& tile : plan.tiles) {
    const auto sx = static_cast<std::int64_t>(x) - tile.x;
    const auto sy = static_cast<std::int64_t>(y) - tile.y;
    if (sx < 0 || sy < 0 || sx >= image_.width || sy >= image_.height) continue;
    const auto* pixel = image_.pixels.data() + sy * image_.pitch + sx * 4;
    if (Nonzero565(pixel, alpha)) return true;
  }
  return false;
}

image_layout::Point GiImage::GetVisualCenter(okgf_rle_bridge::Rect bounds,
                                             okgf_rle_bridge::Rect clip,
                                             image_layout::XMode x_mode,
                                             image_layout::YMode y_mode,
                                             std::uint8_t alpha) const {
  if (!loaded() || !alpha) return {};
  const auto plan = image_layout::MakePlan(bounds, clip, image_.width, image_.height, x_mode, y_mode);
  std::uint64_t count{};
  std::int64_t sum_x{}, sum_y{};
  for (const auto& tile : plan.tiles) {
    const auto left = std::max({tile.x, bounds.left, clip.left});
    const auto top = std::max({tile.y, bounds.top, clip.top});
    const auto right = std::min({static_cast<std::int64_t>(tile.x) + image_.width,
                                 static_cast<std::int64_t>(bounds.right), static_cast<std::int64_t>(clip.right)});
    const auto bottom = std::min({static_cast<std::int64_t>(tile.y) + image_.height,
                                  static_cast<std::int64_t>(bounds.bottom), static_cast<std::int64_t>(clip.bottom)});
    for (std::int64_t y = top; y < bottom; ++y)
      for (std::int64_t x = left; x < right; ++x) {
        const auto* pixel = image_.pixels.data() + (y - tile.y) * image_.pitch + (x - tile.x) * 4;
        if (!Nonzero565(pixel, alpha)) continue;
        const auto local_x = x - bounds.left;
        const auto local_y = y - bounds.top;
        if (count == static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
            sum_x > std::numeric_limits<std::int64_t>::max() - local_x ||
            sum_y > std::numeric_limits<std::int64_t>::max() - local_y) return {};
        ++count; sum_x += local_x; sum_y += local_y;
      }
  }
  return count ? image_layout::Point{static_cast<std::int32_t>(sum_x / static_cast<std::int64_t>(count)),
                                      static_cast<std::int32_t>(sum_y / static_cast<std::int64_t>(count))}
               : image_layout::Point{};
}

}  // namespace srhd_awa::platform::gi_image_cpu

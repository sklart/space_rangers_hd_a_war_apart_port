#include "graph_buffer_cpu.hpp"

#include "gi_image_cpu.hpp"
#include "image_cpu.hpp"
#include "software_compositor.hpp"
#include "okgf.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>

namespace srhd_awa::platform::graph_buffer_cpu {
namespace {
constexpr std::uint64_t kMaxOwnedBytes = 128ull * 1024ull * 1024ull;
constexpr std::int32_t kMaxDimension = 8192;

bool Fail(std::string* error, const char* reason) {
  if (error) *error = reason;
  return false;
}

bool Extent(std::int32_t width, std::int32_t height, std::int32_t bpp,
            std::int32_t* pitch, std::size_t* bytes) {
  if (width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension)
    return false;
  const auto wide_pitch = static_cast<std::uint64_t>(width) * bpp;
  const auto wide_bytes = wide_pitch * height;
  if (wide_pitch > std::numeric_limits<std::int32_t>::max() ||
      wide_bytes > kMaxOwnedBytes) return false;
  *pitch = static_cast<std::int32_t>(wide_pitch);
  *bytes = static_cast<std::size_t>(wide_bytes);
  return true;
}

std::int32_t RoundRatio(std::int64_t numerator, std::int32_t denominator) {
  const auto whole = numerator / denominator;
  const auto remainder = numerator % denominator;
  const auto doubled = remainder * 2;
  return static_cast<std::int32_t>(whole +
      (doubled > denominator || (doubled == denominator && (whole & 1))));
}

std::uint16_t Pack565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  return static_cast<std::uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}

std::uint8_t Blend(std::uint8_t source, std::uint8_t destination, std::uint8_t alpha) {
  return static_cast<std::uint8_t>((static_cast<std::uint32_t>(source) * alpha +
                                    static_cast<std::uint32_t>(destination) * (255 - alpha) +
                                    127) / 255);
}

bool ValidRect(okgf_rle_bridge::Rect bounds) {
  return bounds.left <= bounds.right && bounds.top <= bounds.bottom;
}

bool BoundedLayout(okgf_rle_bridge::Rect bounds, okgf_rle_bridge::Rect clip) {
  // image_layout::MakePlan materializes every tile; reject pathological geometry first.
  constexpr std::int64_t kMaxLayoutSpan = 16384;
  constexpr std::int64_t kCoordinateGuard = 32768;
  const auto safe_coordinate = [](std::int32_t value) {
    return static_cast<std::int64_t>(value) >=
               static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) + kCoordinateGuard &&
           static_cast<std::int64_t>(value) <=
               static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) - kCoordinateGuard;
  };
  return ValidRect(bounds) && ValidRect(clip) &&
      safe_coordinate(bounds.left) && safe_coordinate(bounds.right) &&
      safe_coordinate(bounds.top) && safe_coordinate(bounds.bottom) &&
      safe_coordinate(clip.left) && safe_coordinate(clip.right) &&
      safe_coordinate(clip.top) && safe_coordinate(clip.bottom) &&
      static_cast<std::int64_t>(bounds.right) - bounds.left <= kMaxLayoutSpan &&
      static_cast<std::int64_t>(bounds.bottom) - bounds.top <= kMaxLayoutSpan &&
      static_cast<std::int64_t>(clip.right) - clip.left <= kMaxLayoutSpan &&
      static_cast<std::int64_t>(clip.bottom) - clip.top <= kMaxLayoutSpan;
}
bool BoundedPlan(okgf_rle_bridge::Rect bounds, std::int32_t width,
                 std::int32_t height, image_layout::XMode x_mode,
                 image_layout::YMode y_mode) {
  constexpr std::uint64_t kMaxTiles = 1u << 20;
  if (width <= 0 || height <= 0) return false;
  const auto span_x = static_cast<std::uint64_t>(bounds.right - bounds.left);
  const auto span_y = static_cast<std::uint64_t>(bounds.bottom - bounds.top);
  const bool fill_x = x_mode == image_layout::XMode::LeftFill ||
                      x_mode == image_layout::XMode::RightFill;
  const bool fill_y = y_mode == image_layout::YMode::TopFill ||
                      y_mode == image_layout::YMode::BottomFill;
  const auto tiles_x = fill_x ? (span_x + width - 1) / width + 1 : 1;
  const auto tiles_y = fill_y ? (span_y + height - 1) / height + 1 : 1;
  return tiles_x <= kMaxTiles && tiles_y <= kMaxTiles / tiles_x;
}
}  // namespace

bool GraphBufferCpu::AllocateBuffer(std::int32_t width, std::int32_t height,
                                    std::string* error) {
  std::int32_t pitch{};
  std::size_t bytes{};
  if (!Extent(width, height, 2, &pitch, &bytes))
    return Fail(error, "GraphBuf RGB565 allocation exceeds limit");
  try {
    std::vector<std::uint8_t> buffer(bytes, 0);
    pixels_ = std::move(buffer);
  } catch (const std::bad_alloc&) {
    return Fail(error, "GraphBuf RGB565 allocation failed");
  }
  width_ = width;
  height_ = height;
  pitch_ = pitch;
  format_ = PixelFormat::RGB565;
  source_has_per_pixel_alpha_ = false;
  if (error) error->clear();
  return true;
}

bool GraphBufferCpu::SetPixel565(std::int32_t x, std::int32_t y, std::uint16_t pixel) {
  if (!owns_buffer() || format_ != PixelFormat::RGB565 || x < 0 || y < 0 ||
      x >= width_ || y >= height_) return false;
  const auto at = static_cast<std::size_t>(y) * pitch_ + static_cast<std::size_t>(x) * 2;
  pixels_[at] = static_cast<std::uint8_t>(pixel);
  pixels_[at + 1] = static_cast<std::uint8_t>(pixel >> 8);
  return true;
}

void GraphBufferCpu::ClearOwnedBuffer() {
  pixels_.clear();
  pixels_.shrink_to_fit();
  width_ = height_ = pitch_ = 0;
  format_ = PixelFormat::RGB565;
  source_has_per_pixel_alpha_ = false;
}

bool GraphBufferCpu::LoadBgraPixels(const std::uint8_t* source, std::int32_t width,
                                    std::int32_t height, std::int32_t source_pitch,
                                    std::string* error) {
  std::int32_t pitch{};
  std::size_t bytes{};
  if (!source || !Extent(width, height, 4, &pitch, &bytes) || source_pitch < pitch ||
      static_cast<std::uint64_t>(source_pitch) * height > kMaxOwnedBytes)
    return Fail(error, "GraphBuf BGRA source is invalid or oversized");
  try {
    std::vector<std::uint8_t> buffer(bytes);
    for (std::int32_t y = 0; y < height; ++y)
      std::copy_n(source + static_cast<std::size_t>(y) * source_pitch, pitch,
                  buffer.data() + static_cast<std::size_t>(y) * pitch);
    pixels_ = std::move(buffer);
  } catch (const std::bad_alloc&) {
    return Fail(error, "GraphBuf BGRA allocation failed");
  }
  width_ = width;
  height_ = height;
  pitch_ = pitch;
  format_ = PixelFormat::BGRA8888;
  source_has_per_pixel_alpha_ = true;
  if (error) error->clear();
  return true;
}

bool GraphBufferCpu::AspectFit(std::int32_t source_width, std::int32_t source_height,
                                std::int32_t target_width, std::int32_t target_height,
                                image_layout::Point* size, std::string* error) {
  if (!size || source_width <= 0 || source_height <= 0 || target_width <= 0 ||
      target_height <= 0 || source_width > kMaxDimension || source_height > kMaxDimension ||
      target_width > kMaxDimension || target_height > kMaxDimension)
    return Fail(error, "GraphBuf aspect-fit dimensions are invalid");
  if (static_cast<std::int64_t>(source_width) * target_height >=
      static_cast<std::int64_t>(source_height) * target_width) {
    *size = {target_width, std::max(1, RoundRatio(
        static_cast<std::int64_t>(target_width) * source_height, source_width))};
  } else {
    *size = {std::max(1, RoundRatio(
        static_cast<std::int64_t>(target_height) * source_width, source_height)), target_height};
  }
  if (error) error->clear();
  return true;
}

bool GraphBufferCpu::ScaleAspectFit(std::int32_t target_width, std::int32_t target_height,
                                     ScaleFilter filter, std::string* error) {
  if (pixels_.empty() || format_ != PixelFormat::BGRA8888)
    return Fail(error, "GraphBuf scaling requires an owned BGRA source");
  image_layout::Point size{};
  return AspectFit(width_, height_, target_width, target_height, &size, error) &&
         Rescale(size.x, size.y, filter, error);
}

bool GraphBufferCpu::Rescale(std::int32_t width, std::int32_t height,
                             ScaleFilter filter, std::string* error) {
  if (width == width_ && height == height_) return true;
  std::int32_t target_pitch{};
  std::size_t target_bytes{};
  if (!Extent(width, height, 4, &target_pitch, &target_bytes))
    return Fail(error, "GraphBuf scaled BGRA allocation exceeds limit");
  if (filter == ScaleFilter::OkgfLanczos3 && (width_ < 3 || height_ < 3))
    return Fail(error, "GraphBuf Lanczos3 source is too small for OKGF taps");
  std::vector<std::uint8_t> output;
  try { output.resize(target_bytes); }
  catch (const std::bad_alloc&) { return Fail(error, "GraphBuf scaled BGRA allocation failed"); }
  if (filter == ScaleFilter::OkgfLanczos3) {
    const auto precision = okgf_get_math_precision();
    okgf_set_math_precision(OKGF_MATH_EXTENDED);
    OKGF_Rescale(output.data(), width, height, target_pitch, pixels_.data(),
                 width_, height_, pitch_, 4, OKGF_RESCALE_LANCZOS3);
    okgf_set_math_precision(precision);
  } else {
    const auto x_step = (static_cast<std::int64_t>(width_ - 1) << 16) / width;
    const auto y_step = (static_cast<std::int64_t>(height_ - 1) << 16) / height;
    std::int64_t y_position = 0;
    for (std::int32_t y = 0; y < height; ++y, y_position += y_step) {
      const auto top_y = static_cast<std::int32_t>(y_position >> 16);
      const auto bottom_y = std::min(top_y + 1, height_ - 1);
      const auto bottom_weight = static_cast<std::uint32_t>(y_position & 0xffff) + 1;
      const auto top_weight = static_cast<std::uint32_t>((~y_position) & 0xffff) + 1;
      std::int64_t x_position = 0;
      for (std::int32_t x = 0; x < width; ++x, x_position += x_step) {
        const auto left_x = static_cast<std::int32_t>(x_position >> 16);
        const auto right_x = std::min(left_x + 1, width_ - 1);
        const auto fraction_x = static_cast<std::uint32_t>(x_position & 0xffff);
        const auto top_right = (top_weight * fraction_x) >> 16;
        const auto top_left = top_weight - top_right;
        const auto bottom_right = (bottom_weight * fraction_x) >> 16;
        const auto bottom_left = bottom_weight - bottom_right;
        const auto* tl = pixels_.data() + static_cast<std::size_t>(top_y) * pitch_ + left_x * 4;
        const auto* tr = pixels_.data() + static_cast<std::size_t>(top_y) * pitch_ + right_x * 4;
        const auto* bl = pixels_.data() + static_cast<std::size_t>(bottom_y) * pitch_ + left_x * 4;
        const auto* br = pixels_.data() + static_cast<std::size_t>(bottom_y) * pitch_ + right_x * 4;
        auto* dst = output.data() + static_cast<std::size_t>(y) * target_pitch + x * 4;
        for (int channel = 0; channel < 4; ++channel)
          dst[channel] = static_cast<std::uint8_t>((tl[channel] * top_left +
              tr[channel] * top_right + bl[channel] * bottom_left +
              br[channel] * bottom_right) >> 16);
      }
    }
  }
  pixels_ = std::move(output);
  width_ = width;
  height_ = height;
  pitch_ = target_pitch;
  if (error) error->clear();
  return true;
}

bool GraphBufferCpu::LoadBitmapBytes(const std::uint8_t* source, std::size_t size,
                                      std::int32_t target_width, std::int32_t target_height,
                                      std::string* error) {
  if (size > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return Fail(error, "GraphBuf bitmap source exceeds decoder limit");
  image_cpu::Image decoded;
  if (!image_cpu::Decode(source, static_cast<std::int32_t>(size),
                         image_cpu::Format::BGRA8888, &decoded, error)) return false;
  GraphBufferCpu next;
  if (!next.LoadBgraPixels(decoded.pixels.data(), decoded.width, decoded.height,
                           decoded.pitch, error) ||
      !next.ScaleAspectFit(target_width, target_height, ScaleFilter::BilinearRGBA, error))
    return false;
  *this = std::move(next);
  return true;
}

bool GraphBufferCpu::LoadGiBytes(const std::uint8_t* source, std::size_t size,
                                  std::int32_t target_width, std::int32_t target_height,
                                  std::string* error) {
  gi_image_cpu::GiImage decoded;
  if (!decoded.LoadBytes(source, size, error)) return false;
  const auto& image = decoded.image();
  if (image.bytes_per_pixel != 4) return Fail(error, "GraphBuf GI decoder did not produce BGRA");
  GraphBufferCpu next;
  if (!next.LoadBgraPixels(image.pixels.data(), image.width, image.height,
                           image.pitch, error) ||
      !next.ScaleAspectFit(target_width, target_height, ScaleFilter::OkgfLanczos3, error))
    return false;
  *this = std::move(next);
  return true;
}

std::uint16_t GraphBufferCpu::RenderedPixel(std::int32_t source_x, std::int32_t source_y,
                                             std::uint16_t destination, bool half_alpha) const {
  const auto* pixel = pixels_.data() + static_cast<std::size_t>(source_y) * pitch_ +
                      static_cast<std::size_t>(source_x) *
                          (format_ == PixelFormat::RGB565 ? 2 : 4);
  if (format_ == PixelFormat::RGB565) {
    const auto source = static_cast<std::uint16_t>(pixel[0] | (pixel[1] << 8));
    return half_alpha ? static_cast<std::uint16_t>(((source >> 1) & 0x7bef) +
                                                     ((destination >> 1) & 0x7bef)) : source;
  }
  const auto alpha = pixel[3];
  if (!alpha) return destination;
  if (alpha == 255) return Pack565(pixel[2], pixel[1], pixel[0]);
  const auto red = static_cast<std::uint8_t>(((destination >> 11) & 31) * 255 / 31);
  const auto green = static_cast<std::uint8_t>(((destination >> 5) & 63) * 255 / 63);
  const auto blue = static_cast<std::uint8_t>((destination & 31) * 255 / 31);
  return Pack565(Blend(pixel[2], red, alpha), Blend(pixel[1], green, alpha),
                 Blend(pixel[0], blue, alpha));
}

bool GraphBufferCpu::Draw(const scene_compositor::Framebuffer& target,
                          okgf_rle_bridge::Rect bounds, okgf_rle_bridge::Rect clip,
                          image_layout::XMode x_mode, image_layout::YMode y_mode,
                          bool half_alpha, std::string* error) const {
  if (!owns_buffer() || !target.pixels || target.width <= 0 || target.height <= 0 ||
      target.pitch_pixels < target.width || !BoundedLayout(bounds, clip) ||
      !BoundedPlan(bounds, width_, height_, x_mode, y_mode) ||
      clip.left < 0 || clip.top < 0 || clip.right > target.width ||
      clip.bottom > target.height)
    return Fail(error, "GraphBuf draw target or clip is invalid");
  if (std::max(bounds.left, clip.left) >= std::min(bounds.right, clip.right) ||
      std::max(bounds.top, clip.top) >= std::min(bounds.bottom, clip.bottom)) {
    if (error) error->clear();
    return true;
  }
  const auto plan = image_layout::MakePlan(bounds, clip, width_, height_, x_mode, y_mode);
  if (!plan.supported) return Fail(error, "GraphBuf layout mode is unsupported");
  const software_compositor::Rect alpha_clip{
      std::max(clip.left, bounds.left), std::max(clip.top, bounds.top),
      std::min(clip.right, bounds.right), std::min(clip.bottom, bounds.bottom)};
  for (const auto& tile : plan.tiles) {
    if (format_ == PixelFormat::BGRA8888) {
      if (!software_compositor::CompositeBGRA(target.pixels, target.width, target.height,
              target.pitch_pixels, pixels_.data(), width_, height_, pitch_, tile.x, tile.y,
              software_compositor::BlendMode::Alpha, &alpha_clip, error)) return false;
      continue;
    }
    const auto left = std::max({tile.x, bounds.left, clip.left});
    const auto top = std::max({tile.y, bounds.top, clip.top});
    const auto right = std::min({static_cast<std::int64_t>(tile.x) + width_,
                                 static_cast<std::int64_t>(bounds.right),
                                 static_cast<std::int64_t>(clip.right)});
    const auto bottom = std::min({static_cast<std::int64_t>(tile.y) + height_,
                                  static_cast<std::int64_t>(bounds.bottom),
                                  static_cast<std::int64_t>(clip.bottom)});
    for (std::int64_t y = top; y < bottom; ++y)
      for (std::int64_t x = left; x < right; ++x) {
        auto& destination = target.pixels[y * target.pitch_pixels + x];
        destination = RenderedPixel(static_cast<std::int32_t>(x - tile.x),
                                    static_cast<std::int32_t>(y - tile.y),
                                    destination, half_alpha);
      }
  }
  if (error) error->clear();
  return true;
}

bool GraphBufferCpu::HitTestPixel(std::int32_t x, std::int32_t y,
                                  okgf_rle_bridge::Rect bounds, okgf_rle_bridge::Rect clip,
                                  image_layout::XMode x_mode, image_layout::YMode y_mode,
                                  bool half_alpha) const {
  if (!owns_buffer() || !BoundedLayout(bounds, clip) ||
      !BoundedPlan(bounds, width_, height_, x_mode, y_mode) ||
      x == std::numeric_limits<std::int32_t>::max() ||
      y == std::numeric_limits<std::int32_t>::max() ||
      x < bounds.left || y < bounds.top || x >= bounds.right ||
      y >= bounds.bottom || x < clip.left || y < clip.top || x >= clip.right || y >= clip.bottom)
    return false;
  const okgf_rle_bridge::Rect pixel_clip{x, y, x + 1, y + 1};
  const auto plan = image_layout::MakePlan(bounds, pixel_clip, width_, height_, x_mode, y_mode);
  std::uint16_t rendered{};
  for (const auto& tile : plan.tiles) {
    const auto sx = static_cast<std::int64_t>(x) - tile.x;
    const auto sy = static_cast<std::int64_t>(y) - tile.y;
    if (sx >= 0 && sy >= 0 && sx < width_ && sy < height_)
      rendered = RenderedPixel(static_cast<std::int32_t>(sx),
                               static_cast<std::int32_t>(sy), rendered, half_alpha);
  }
  return rendered != 0;
}

image_layout::Point GraphBufferCpu::GetVisualCenter(okgf_rle_bridge::Rect bounds,
                                                     okgf_rle_bridge::Rect clip,
                                                     image_layout::XMode x_mode,
                                                     image_layout::YMode y_mode,
                                                     bool half_alpha) const {
  if (!owns_buffer() || !BoundedLayout(bounds, clip) ||
      !BoundedPlan(bounds, width_, height_, x_mode, y_mode) ||
      std::max(bounds.left, clip.left) >= std::min(bounds.right, clip.right) ||
      std::max(bounds.top, clip.top) >= std::min(bounds.bottom, clip.bottom)) return {};
  const auto plan = image_layout::MakePlan(bounds, clip, width_, height_, x_mode, y_mode);
  if (!plan.supported) return {};
  std::int64_t sum_x{}, sum_y{}, count{};
  for (const auto& tile : plan.tiles) {
    const auto left = std::max({tile.x, bounds.left, clip.left});
    const auto top = std::max({tile.y, bounds.top, clip.top});
    const auto right = std::min({static_cast<std::int64_t>(tile.x) + width_,
                                 static_cast<std::int64_t>(bounds.right),
                                 static_cast<std::int64_t>(clip.right)});
    const auto bottom = std::min({static_cast<std::int64_t>(tile.y) + height_,
                                  static_cast<std::int64_t>(bounds.bottom),
                                  static_cast<std::int64_t>(clip.bottom)});
    for (std::int64_t y = top; y < bottom; ++y)
      for (std::int64_t x = left; x < right; ++x)
        if (RenderedPixel(static_cast<std::int32_t>(x - tile.x),
                          static_cast<std::int32_t>(y - tile.y), 0, half_alpha)) {
          ++count;
          sum_x += x - bounds.left;
          sum_y += y - bounds.top;
        }
  }
  return count ? image_layout::Point{static_cast<std::int32_t>(sum_x / count),
                                      static_cast<std::int32_t>(sum_y / count)}
               : image_layout::Point{};
}

}  // namespace srhd_awa::platform::graph_buffer_cpu

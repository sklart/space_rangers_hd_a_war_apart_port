#pragma once

#include "image_layout.hpp"
#include "okgf_rle_bridge.hpp"
#include "scene_compositor.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::graph_buffer_cpu {

enum class PixelFormat { RGB565, BGRA8888 };
enum class ScaleFilter { BilinearRGBA, OkgfLanczos3 };

// Owned CPU buffer corresponding to the config-backed subset of GraphBuf.
// External/borrowed buffers and screen capture are deliberately separate APIs.
class GraphBufferCpu {
 public:
  bool AllocateBuffer(std::int32_t width, std::int32_t height,
                      std::string* error = nullptr);
  bool SetPixel565(std::int32_t x, std::int32_t y, std::uint16_t pixel);
  void ClearOwnedBuffer();
  bool LoadBitmapBytes(const std::uint8_t* source, std::size_t size,
                       std::int32_t target_width, std::int32_t target_height,
                       std::string* error = nullptr);
  bool LoadGiBytes(const std::uint8_t* source, std::size_t size,
                   std::int32_t target_width, std::int32_t target_height,
                   std::string* error = nullptr);
  bool LoadBgraPixels(const std::uint8_t* source, std::int32_t width,
                      std::int32_t height, std::int32_t pitch,
                      std::string* error = nullptr);
  bool ScaleAspectFit(std::int32_t target_width, std::int32_t target_height,
                      ScaleFilter filter, std::string* error = nullptr);
  bool Draw(const scene_compositor::Framebuffer& target,
            okgf_rle_bridge::Rect bounds, okgf_rle_bridge::Rect clip,
            image_layout::XMode x_mode, image_layout::YMode y_mode,
            bool half_alpha, std::string* error = nullptr) const;
  bool HitTestPixel(std::int32_t x, std::int32_t y,
                    okgf_rle_bridge::Rect bounds, okgf_rle_bridge::Rect clip,
                    image_layout::XMode x_mode, image_layout::YMode y_mode,
                    bool half_alpha) const;
  image_layout::Point GetVisualCenter(okgf_rle_bridge::Rect bounds,
                                       okgf_rle_bridge::Rect clip,
                                       image_layout::XMode x_mode,
                                       image_layout::YMode y_mode,
                                       bool half_alpha) const;

  std::int32_t width() const { return width_; }
  std::int32_t height() const { return height_; }
  std::int32_t pitch() const { return pitch_; }
  PixelFormat pixel_format() const { return format_; }
  bool owns_buffer() const { return !pixels_.empty(); }
  bool source_has_per_pixel_alpha() const { return source_has_per_pixel_alpha_; }
  std::size_t bytes() const { return pixels_.size(); }
  const std::vector<std::uint8_t>& pixels() const { return pixels_; }
  static bool AspectFit(std::int32_t source_width, std::int32_t source_height,
                        std::int32_t target_width, std::int32_t target_height,
                        image_layout::Point* size, std::string* error = nullptr);

 private:
  bool Rescale(std::int32_t width, std::int32_t height, ScaleFilter filter,
               std::string* error);
  std::uint16_t RenderedPixel(std::int32_t source_x, std::int32_t source_y,
                              std::uint16_t destination, bool half_alpha) const;
  std::int32_t width_{}, height_{}, pitch_{};
  PixelFormat format_{PixelFormat::RGB565};
  bool source_has_per_pixel_alpha_{};
  std::vector<std::uint8_t> pixels_;
};

}  // namespace srhd_awa::platform::graph_buffer_cpu

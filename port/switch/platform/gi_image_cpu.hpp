#pragma once

#include "gi_format0_cpu.hpp"
#include "image_layout.hpp"
#include "okgf_rle_bridge.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace srhd_awa::package { class Package; }
namespace srhd_awa::platform::gi_image_cpu {

// Raw, static GI resource. GAI containers belong to gai_cpu/GIObject instead.
class GiImage {
 public:
  bool LoadBytes(const std::uint8_t* bytes, std::size_t size, std::string* error = nullptr);
  bool Load(package::Package& package, const std::string& resource, std::string* error = nullptr);
  bool Draw(std::uint16_t* pixels, std::int32_t width, std::int32_t height,
            std::int32_t pitch, okgf_rle_bridge::Rect bounds,
            okgf_rle_bridge::Rect clip, image_layout::XMode x_mode,
            image_layout::YMode y_mode, std::uint8_t alpha = 255,
            std::string* error = nullptr) const;
  bool HitTestPixel(std::int32_t x, std::int32_t y,
                    okgf_rle_bridge::Rect bounds, okgf_rle_bridge::Rect clip,
                    image_layout::XMode x_mode, image_layout::YMode y_mode,
                    std::uint8_t alpha = 255) const;
  image_layout::Point GetVisualCenter(okgf_rle_bridge::Rect bounds,
                                      okgf_rle_bridge::Rect clip,
                                      image_layout::XMode x_mode,
                                      image_layout::YMode y_mode,
                                      std::uint8_t alpha = 255) const;
  bool loaded() const { return image_.width > 0; }
  std::int32_t width() const { return image_.width; }
  std::int32_t height() const { return image_.height; }
  std::int32_t origin_x() const { return origin_x_; }
  std::int32_t origin_y() const { return origin_y_; }
  std::int32_t format() const { return format_; }
  const gi_format0_cpu::CpuImage& image() const { return image_; }

 private:
  gi_format0_cpu::CpuImage image_;
  std::int32_t origin_x_{}, origin_y_{}, format_{};
};

}  // namespace srhd_awa::platform::gi_image_cpu

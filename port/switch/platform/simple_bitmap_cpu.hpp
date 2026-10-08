#pragma once

#include "okgf_rle_bridge.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::simple_bitmap_cpu {

class SimpleBitmap {
 public:
  bool Load(const std::uint8_t* source, std::int32_t source_size, bool source_rgba,
            std::string* error = nullptr);
  void Clear();
  bool Draw(std::uint16_t* destination, std::int32_t destination_width,
            std::int32_t destination_height, std::int32_t pitch_pixels, std::int32_t x,
            std::int32_t y, bool half_alpha, const okgf_rle_bridge::Rect& clip,
            std::string* error = nullptr) const;
  std::int32_t width() const { return width_; }
  std::int32_t height() const { return height_; }
  bool source_rgba() const { return source_rgba_; }
  bool loaded() const { return !pixels_.empty(); }
  std::size_t resident_bytes() const { return pixels_.size(); }
 private:
  std::int32_t width_{}, height_{}, pitch_{};
  bool source_rgba_{};
  std::vector<std::uint8_t> pixels_;
};

}  // namespace srhd_awa::platform::simple_bitmap_cpu

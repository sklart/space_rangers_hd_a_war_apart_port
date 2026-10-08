#pragma once

#include "okgf_rle_bridge.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::trans_bitmap_cpu {

class TransBitmap {
 public:
  bool Load(const std::uint8_t* source, std::int32_t source_size, const std::string& operations,
            std::string* error = nullptr);
  void Clear();
  bool Draw(std::uint16_t* destination, std::int32_t destination_width,
            std::int32_t destination_height, std::int32_t pitch_pixels, std::int32_t x,
            std::int32_t y, bool half_alpha, const okgf_rle_bridge::Rect& clip,
            std::string* error = nullptr) const;
  std::int32_t width() const { return width_; }
  std::int32_t height() const { return height_; }
  bool loaded() const { return !rle_.empty(); }
  std::size_t resident_bytes() const { return rle_.size(); }
 private:
  std::int32_t width_{};
  std::int32_t height_{};
  std::vector<std::uint8_t> rle_;
};

}  // namespace srhd_awa::platform::trans_bitmap_cpu

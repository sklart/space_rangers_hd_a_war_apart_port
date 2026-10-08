#pragma once

#include "okgf_rle_bridge.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::alpha_bitmap_cpu {

bool ValidateRle(const std::uint8_t* bytes, std::size_t byte_count, std::int32_t width,
                 std::int32_t height, std::size_t literal_bytes);

class AlphaBitmap {
 public:
  bool Load(const std::uint8_t* source, std::int32_t source_size,
            std::string* error = nullptr);
  void Clear();
  bool Draw(std::uint16_t* destination, std::int32_t destination_width,
            std::int32_t destination_height, std::int32_t pitch_pixels,
            std::int32_t x, std::int32_t y, const okgf_rle_bridge::Rect& clip,
            std::string* error = nullptr) const;

  std::int32_t width() const { return width_; }
  std::int32_t height() const { return height_; }
  std::size_t resident_bytes() const { return trans_.size() + trans_alpha_.size() + alpha_.size(); }
  bool loaded() const { return !trans_.empty() && !trans_alpha_.empty() && !alpha_.empty(); }

 private:
  std::int32_t width_{};
  std::int32_t height_{};
  std::vector<std::uint8_t> trans_;
  std::vector<std::uint8_t> trans_alpha_;
  std::vector<std::uint8_t> alpha_;
};

}  // namespace srhd_awa::platform::alpha_bitmap_cpu

#pragma once

#include "aft_font.hpp"
#include "okgf_rle_bridge.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace srhd_awa::platform::font_renderer {

struct Target {
  std::uint16_t* pixels{};
  std::int32_t width{}, height{}, pitch_pixels{};
  okgf_rle_bridge::Rect clip{}; // half-open
};

bool DrawGlyph(const aft_font::AftFont& font, char16_t code, Target target,
               std::int32_t x, std::int32_t baseline_y, std::uint16_t color,
               std::string* error = nullptr);
bool DrawPlainLine(const aft_font::AftFont& font, std::u16string_view text, Target target,
                   std::int32_t x, std::int32_t baseline_y, std::uint16_t color,
                   std::string* error = nullptr);

}  // namespace srhd_awa::platform::font_renderer

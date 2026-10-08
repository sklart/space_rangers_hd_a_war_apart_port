#include "font_renderer.hpp"

#include <climits>
#include <cstdint>

namespace srhd_awa::platform::font_renderer {
namespace {
bool Fail(std::string* error, const char* why) { if (error) *error = why; return false; }
bool Valid(Target target) {
  return target.pixels && target.width > 0 && target.height > 0 &&
         target.pitch_pixels >= target.width && target.pitch_pixels <= INT32_MAX / 2 &&
         target.clip.left >= 0 && target.clip.top >= 0 &&
         target.clip.right <= target.width && target.clip.bottom <= target.height &&
         target.clip.left <= target.clip.right && target.clip.top <= target.clip.bottom;
}
bool DrawPlane(const aft_font::AftFont& font, const aft_font::GlyphPlane& plane,
               bool alpha, Target target, std::int64_t x, std::int64_t y,
               std::uint16_t color, std::string* error) {
  if (!plane.data_offset) return true;
  x += plane.left; y += plane.top;
  if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX ||
      x + plane.width > INT32_MAX || y + plane.height > INT32_MAX)
    return Fail(error, "glyph position overflow");
  const auto pitch_bytes = target.pitch_pixels * 2;
  if (alpha) okgf_rle_bridge::FillAlpha565Clip(target.pixels, pitch_bytes,
      static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
      font.PlaneData(plane), color, target.clip);
  else okgf_rle_bridge::DrawMask565Clip(target.pixels, pitch_bytes,
      static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
      font.PlaneData(plane), color, target.clip);
  return true;
}
}  // namespace

bool DrawGlyph(const aft_font::AftFont& font, char16_t code, Target target,
               std::int32_t x, std::int32_t baseline_y, std::uint16_t color,
               std::string* error) {
  if (!Valid(target)) return Fail(error, "invalid font framebuffer or clip");
  if (target.clip.left == target.clip.right || target.clip.top == target.clip.bottom) return true;
  const auto* glyph = font.Find(code);
  if (!glyph) return true;
  return DrawPlane(font, glyph->opaque, false, target, x, baseline_y, color, error) &&
         DrawPlane(font, glyph->alpha, true, target, x, baseline_y, color, error);
}

bool DrawPlainLine(const aft_font::AftFont& font, std::u16string_view text, Target target,
                   std::int32_t x, std::int32_t baseline_y, std::uint16_t color,
                   std::string* error) {
  if (!Valid(target)) return Fail(error, "invalid font framebuffer or clip");
  std::int64_t pen = x;
  for (char16_t code : text) {
    if (pen < INT32_MIN || pen > INT32_MAX) return Fail(error, "font pen overflow");
    if (!DrawGlyph(font, code, target, static_cast<std::int32_t>(pen), baseline_y, color, error)) return false;
    pen += font.Advance(code);
  }
  if (error) error->clear();
  return true;
}

}  // namespace srhd_awa::platform::font_renderer

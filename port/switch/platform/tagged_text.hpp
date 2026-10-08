#pragma once

#include "aft_font.hpp"
#include "font_renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace srhd_awa::platform::tagged_text {

enum class TokenKind { Glyph, ColorStart, ColorEnd, Object, FixStart, FixEnd,
                       FormatStart, FormatEnd, Tab, AlignStart, AlignEnd, Unknown };
struct Token {
  TokenKind kind{TokenKind::Glyph};
  std::size_t start{}, length{};
  char16_t code{};
  std::int32_t a{}, b{}, c{}, d{};
};
struct Bounds { std::int32_t left{}, top{}, right{}, bottom{}; };
struct GlyphPlacement {
  char16_t code{};
  std::int32_t x{};
  std::uint16_t color{};
  bool justify_gap{};
};
struct ObjectPlacement {
  std::int32_t id{}, width{}, height{}, vertical_mode{}, x{}, y{};
};
struct Layout {
  std::vector<GlyphPlacement> glyphs;
  std::vector<ObjectPlacement> objects;
  Bounds bounds{};
  std::int32_t advance{}, top_adjustment{};
};
struct Options {
  std::uint16_t default_color{0xffff};
  bool color_tags_enabled{true};
  bool reject_unknown_tags{true};
};

std::uint16_t PackRgb565(std::uint32_t red, std::uint32_t green, std::uint32_t blue);
bool Tokenize(std::u16string_view text, std::vector<Token>* tokens,
              std::string* error = nullptr, bool reject_unknown_tags = true);
bool LayoutLine(const aft_font::AftFont& font, std::u16string_view text,
                const Options& options, Layout* layout, std::string* error = nullptr);
bool MeasureTaggedTextBounds(const aft_font::AftFont& font, std::u16string_view text,
                             Bounds* bounds, std::int32_t* top_adjustment = nullptr,
                             std::string* error = nullptr);
bool WrapTaggedTextIntoLines(const aft_font::AftFont& font, std::u16string_view text,
                             std::int32_t max_width, std::vector<std::u16string>* lines,
                             std::string* error = nullptr);
bool DrawLayout(const aft_font::AftFont& font, const Layout& layout,
                font_renderer::Target target, std::int32_t x, std::int32_t baseline_y,
                std::string* error = nullptr);
bool DrawJustifiedLayout(const aft_font::AftFont& font, const Layout& layout,
                         font_renderer::Target target, std::int32_t x,
                         std::int32_t baseline_y, std::int32_t width,
                         bool use_tagged_color, std::uint16_t solid_color,
                         std::string* error = nullptr);
bool DrawJustifiedTaggedText16(const aft_font::AftFont& font, std::u16string_view text,
                               const Options& options, font_renderer::Target target,
                               std::int32_t x, std::int32_t baseline_y, std::int32_t width,
                               std::string* error = nullptr);

}  // namespace srhd_awa::platform::tagged_text

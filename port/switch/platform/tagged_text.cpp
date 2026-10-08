#include "tagged_text.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <limits>
#include <new>

namespace srhd_awa::platform::tagged_text {
namespace {
bool Fail(std::string* error, const char* reason) { if (error) *error = reason; return false; }
char16_t Lower(char16_t c) { return c >= u'A' && c <= u'Z' ? c + 32 : c; }
bool Equal(std::u16string_view a, std::u16string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) if (Lower(a[i]) != Lower(b[i])) return false;
  return true;
}
bool Starts(std::u16string_view a, std::u16string_view b) {
  return a.size() >= b.size() && Equal(a.substr(0, b.size()), b);
}
bool Number(std::u16string_view text, std::int32_t* result) {
  std::int64_t value = 0;
  for (char16_t c : text) {
    if (c < u'0' || c > u'9') return false;
    value = value * 10 + c - u'0';
    if (value > INT32_MAX) return false;
  }
  *result = static_cast<std::int32_t>(value);
  return true;
}
bool Fields(std::u16string_view text, std::int32_t* values, std::size_t count) {
  std::size_t start = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const auto end = text.find(u',', start);
    if (i + 1 == count && end != std::u16string_view::npos) return false;
    if (i + 1 != count && end == std::u16string_view::npos) return false;
    if (!Number(text.substr(start, end == std::u16string_view::npos ? end : end - start), &values[i])) return false;
    start = end == std::u16string_view::npos ? text.size() : end + 1;
  }
  return true;
}
bool Narrow(std::int64_t n, std::int32_t* out) {
  if (n < INT32_MIN || n > INT32_MAX) return false;
  *out = static_cast<std::int32_t>(n); return true;
}
bool Recognize(std::u16string_view body, Token* token) {
  if (Equal(body, u"/color")) { token->kind = TokenKind::ColorEnd; return true; }
  if (Equal(body, u"fix")) { token->kind = TokenKind::FixStart; return true; }
  if (Equal(body, u"/fix")) { token->kind = TokenKind::FixEnd; return true; }
  if (Equal(body, u"/format")) { token->kind = TokenKind::FormatEnd; return true; }
  if (Equal(body, u"/align")) { token->kind = TokenKind::AlignEnd; return true; }
  if (Starts(body, u"color=")) {
    std::int32_t rgb[3]{};
    if (!Fields(body.substr(6), rgb, 3)) return false;
    token->kind = TokenKind::ColorStart;
    token->a = rgb[0]; token->b = rgb[1]; token->c = rgb[2];
    return true;
  }
  if (Starts(body, u"object=")) {
    std::int32_t fields[4]{};
    if (!Fields(body.substr(7), fields, 4)) return false;
    token->kind = TokenKind::Object;
    token->a = fields[0]; token->b = fields[1]; token->c = fields[2]; token->d = fields[3];
    return true;
  }
  if (Starts(body, u"td=")) {
    if (!Number(body.substr(3), &token->a)) return false;
    token->kind = TokenKind::Tab; return true;
  }
  if (Starts(body, u"format=")) {
    const auto value = body.substr(7);
    const auto comma = value.find(u',');
    if (comma == std::u16string_view::npos || !Number(value.substr(comma + 1), &token->a)) return false;
    const auto align = value.substr(0, comma);
    if (Equal(align, u"left")) token->b = -1;
    else if (Equal(align, u"center")) token->b = 0;
    else if (Equal(align, u"right")) token->b = 1;
    else return false;
    token->kind = TokenKind::FormatStart; return true;
  }
  if (Starts(body, u"align=")) {
    const auto align = body.substr(6);
    // TCFontEC's uppercase value checks index the prefix, so uppercase values
    // fail for ordinary release spelling. Keep that observable quirk.
    if (align == u"right") token->a = 1;
    else if (align == u"center") token->a = 0;
    else return false;
    token->kind = TokenKind::AlignStart; return true;
  }
  return false;
}
std::int64_t Advance(const aft_font::AftFont& font, char16_t code, int fixed_depth) {
  if (!font.Find(code)) return 0;
  return fixed_depth ? font.max_glyph_advance() : font.Advance(code);
}
std::int64_t Space(const aft_font::AftFont& font, int fixed_depth) {
  return fixed_depth ? font.max_glyph_advance() : font.Advance(u' ');
}
std::int64_t AlignRemaining(const aft_font::AftFont& font, const std::vector<Token>& tokens,
                            std::size_t at) {
  std::int64_t width = 0;
  for (std::size_t i = at + 1; i < tokens.size(); ++i) {
    if (tokens[i].kind == TokenKind::AlignEnd) break;
    if (tokens[i].kind == TokenKind::Glyph) width += font.Advance(tokens[i].code);
    if (tokens[i].kind == TokenKind::Object) width += tokens[i].b;
  }
  return width;
}
std::int32_t VisibleUntilFormatEnd(const aft_font::AftFont& font,
                                    const std::vector<Token>& tokens, std::size_t at) {
  std::int32_t count = 0;
  for (std::size_t i = at + 1; i < tokens.size(); ++i) {
    if (tokens[i].kind == TokenKind::FormatEnd) break;
    if (tokens[i].kind == TokenKind::Glyph && font.Find(tokens[i].code)) ++count;
  }
  return count;
}
bool Build(const aft_font::AftFont& font, const std::vector<Token>& tokens,
           const Options& options, bool measure_mode, Layout* layout, std::string* error) {
  Layout result;
  std::vector<std::uint16_t> colors;
  std::int64_t pen = 0;
  std::int32_t fixed_depth = 0, format_left = -1;
  bool has_plane = false, leading = true;
  std::int64_t left = INT64_MAX, right = INT64_MIN, top = INT64_MAX, bottom = INT64_MIN;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const auto& token = tokens[i];
    switch (token.kind) {
      case TokenKind::Glyph: {
        const auto* glyph = font.Find(token.code);
        if (!glyph) break;
        if (measure_mode && format_left > 0) --format_left;
        const auto color = options.color_tags_enabled && !colors.empty() ? colors.back() : options.default_color;
        std::int32_t x{};
        if (!Narrow(pen, &x)) return Fail(error, "tagged text pen overflow");
        const bool justify_gap = token.code == u' ' && !leading &&
                                 format_left < 0 && fixed_depth == 0;
        result.glyphs.push_back({token.code, x, color, justify_gap});
        for (const auto& plane : {glyph->opaque, glyph->alpha}) if (plane.data_offset) {
          has_plane = true;
          left = std::min(left, pen + plane.left); right = std::max(right, pen + plane.left + plane.width);
          top = std::min<std::int64_t>(top, plane.top); bottom = std::max<std::int64_t>(bottom, plane.top + plane.height);
        }
        pen += Advance(font, token.code, fixed_depth);
        if (token.code != u' ' || fixed_depth > 0 || format_left > 0) leading = false;
        break;
      }
      case TokenKind::ColorStart:
        if (options.color_tags_enabled) colors.push_back(PackRgb565(token.a, token.b, token.c));
        break;
      case TokenKind::ColorEnd:
        if (options.color_tags_enabled && !colors.empty()) colors.pop_back();
        break;
      case TokenKind::FixStart: ++fixed_depth; break;
      case TokenKind::FixEnd: fixed_depth = std::max(0, fixed_depth - 1); break;
      case TokenKind::Tab: pen = std::max<std::int64_t>(pen, token.a); break;
      case TokenKind::AlignStart: {
        const auto width = AlignRemaining(font, tokens, i);
        pen -= token.a == 1 ? width : width / 2;
        break;
      }
      case TokenKind::AlignEnd: break;
      case TokenKind::FormatStart: {
        if (format_left >= 0) break;
        format_left = measure_mode ? token.a : token.a - VisibleUntilFormatEnd(font, tokens, i);
        if (format_left > 0 && !measure_mode) {
          if (token.b == 1) { pen += Space(font, fixed_depth) * format_left; format_left = -1; }
          else if (token.b == 0) {
            const auto half = format_left / 2;
            pen += Space(font, fixed_depth) * half;
            format_left -= half;
          }
        }
        break;
      }
      case TokenKind::FormatEnd:
        if (format_left > 0) pen += Space(font, fixed_depth) * format_left;
        format_left = -1; break;
      case TokenKind::Object: {
        std::int32_t x{};
        if (!Narrow(pen, &x)) return Fail(error, "tagged object position overflow");
        result.objects.push_back({token.a, token.b, token.c, token.d, x, 0});
        pen += token.b;
        leading = false;
        break;
      }
      case TokenKind::Unknown:
        if (options.reject_unknown_tags) return Fail(error, "unknown tagged text token");
        break;
    }
    if (pen < INT32_MIN || pen > INT32_MAX) return Fail(error, "tagged text width overflow");
  }
  if (format_left > 0) pen += Space(font, fixed_depth) * format_left;
  if (pen < INT32_MIN || pen > INT32_MAX) return Fail(error, "tagged text width overflow");
  right = std::max(right, pen);
  if (!has_plane) { left = std::min<std::int64_t>(0, pen); top = bottom = 0; }
  const auto middle = (top + bottom) / 2;
  for (auto& object : result.objects) if (object.vertical_mode == 0) {
    object.y = static_cast<std::int32_t>(middle - object.height / 2);
    if (object.y < top) result.top_adjustment += static_cast<std::int32_t>(top - object.y);
    top = std::min<std::int64_t>(top, object.y);
    bottom = std::max<std::int64_t>(bottom, std::int64_t(object.y) + object.height);
  }
  if (!Narrow(left, &result.bounds.left) || !Narrow(right, &result.bounds.right) ||
      !Narrow(top, &result.bounds.top) || !Narrow(bottom, &result.bounds.bottom))
    return Fail(error, "tagged text bounds overflow");
  result.advance = static_cast<std::int32_t>(pen);
  *layout = std::move(result);
  return true;
}
}  // namespace

std::uint16_t PackRgb565(std::uint32_t red, std::uint32_t green, std::uint32_t blue) {
  const auto r = static_cast<std::uint8_t>(red), g = static_cast<std::uint8_t>(green),
             b = static_cast<std::uint8_t>(blue);
  return static_cast<std::uint16_t>(((std::uint32_t(r) * 31 / 255) << 11) |
                                    ((std::uint32_t(g) * 63 / 255) << 5) |
                                    (std::uint32_t(b) * 31 / 255));
}

bool Tokenize(std::u16string_view text, std::vector<Token>* tokens,
              std::string* error, bool reject_unknown_tags) {
  if (!tokens) return Fail(error, "null tagged token output");
  tokens->clear();
  if (text.size() > 1000000) return Fail(error, "tagged text is too long");
  try {
    for (std::size_t i = 0; i < text.size();) {
      if (text[i] == u'<' && i + 1 < text.size() && text[i + 1] != u'<') {
        const auto end = text.find(u'>', i + 1);
        if (end != std::u16string_view::npos) {
          Token token{}; token.start = i; token.length = end - i + 1;
          if (!Recognize(text.substr(i + 1, end - i - 1), &token)) token.kind = TokenKind::Unknown;
          if (token.kind == TokenKind::Unknown && reject_unknown_tags)
            return Fail(error, "unknown tagged text token");
          tokens->push_back(token); i = end + 1; continue;
        }
      }
      tokens->push_back({TokenKind::Glyph, i, 1, text[i]}); ++i;
    }
    if (error) error->clear();
    return true;
  } catch (const std::bad_alloc&) { tokens->clear(); return Fail(error, "tagged token allocation failed"); }
}

bool LayoutLine(const aft_font::AftFont& font, std::u16string_view text,
                const Options& options, Layout* layout, std::string* error) {
  if (!layout) return Fail(error, "null tagged layout output");
  *layout = {};
  std::vector<Token> tokens;
  return Tokenize(text, &tokens, error, options.reject_unknown_tags) &&
         Build(font, tokens, options, false, layout, error);
}

bool MeasureTaggedTextBounds(const aft_font::AftFont& font, std::u16string_view text,
                             Bounds* bounds, std::int32_t* top_adjustment, std::string* error) {
  if (!bounds) return Fail(error, "null tagged bounds output");
  *bounds = {};
  if (top_adjustment) *top_adjustment = 0;
  std::vector<Token> tokens;
  Layout layout;
  if (!Tokenize(text, &tokens, error) || !Build(font, tokens, {}, true, &layout, error)) return false;
  *bounds = layout.bounds;
  if (top_adjustment) *top_adjustment = layout.top_adjustment;
  return true;
}

bool DrawLayout(const aft_font::AftFont& font, const Layout& layout,
                font_renderer::Target target, std::int32_t x, std::int32_t baseline_y,
                std::string* error) {
  for (const auto& glyph : layout.glyphs) {
    std::int32_t at{};
    if (!Narrow(std::int64_t(x) + glyph.x, &at)) return Fail(error, "tagged draw position overflow");
    if (!font_renderer::DrawGlyph(font, glyph.code, target, at, baseline_y, glyph.color, error)) return false;
  }
  return true;
}

bool WrapTaggedTextIntoLines(const aft_font::AftFont& font, std::u16string_view text,
                             std::int32_t max_width, std::vector<std::u16string>* lines,
                             std::string* error) {
  if (!lines || max_width <= 0) return Fail(error, "invalid tagged wrap width or output");
  lines->clear();
  std::vector<Token> tokens;
  if (!Tokenize(text, &tokens, error)) return false;
  try {
    std::size_t begin = 0;
    while (begin < tokens.size()) {
      std::size_t end = begin, best = begin;
      std::int64_t width = 0;
      int fixed = 0;
      for (; end < tokens.size(); ++end) {
        const auto& token = tokens[end];
        if (token.kind == TokenKind::FixStart) ++fixed;
        if (token.kind == TokenKind::FixEnd) fixed = std::max(0, fixed - 1);
        const auto next = width + (token.kind == TokenKind::Glyph ? Advance(font, token.code, fixed) :
                                   token.kind == TokenKind::Object ? token.b : 0);
        if (next > max_width && end > begin) break;
        width = next;
        if (token.kind == TokenKind::Glyph && token.code == u' ') best = end + 1;
        if (next > max_width) { ++end; break; }
      }
      if (end == begin) ++end;
      if (end < tokens.size() && best > begin) end = best;
      const auto first = tokens[begin].start;
      const auto last = tokens[end - 1].start + tokens[end - 1].length;
      auto line = std::u16string(text.substr(first, last - first));
      if (!lines->empty()) {
        const auto start = line.find_first_not_of(u' ');
        line.erase(0, start == std::u16string::npos ? line.size() : start);
      }
      lines->push_back(std::move(line));
      begin = end;
    }
    if (error) error->clear();
    return true;
  } catch (const std::bad_alloc&) { lines->clear(); return Fail(error, "tagged wrap allocation failed"); }
}

bool DrawJustifiedLayout(const aft_font::AftFont& font, const Layout& layout,
                         font_renderer::Target target, std::int32_t x,
                         std::int32_t baseline_y, std::int32_t width,
                         bool use_tagged_color, std::uint16_t solid_color,
                         std::string* error) {
  const auto draw_normal = [&]() {
    for (const auto& glyph : layout.glyphs) {
      std::int32_t at{};
      if (!Narrow(std::int64_t(x) + glyph.x, &at))
        return Fail(error, "justified fallback position overflow");
      if (!font_renderer::DrawGlyph(font, glyph.code, target, at, baseline_y,
                                    use_tagged_color ? glyph.color : solid_color, error)) return false;
    }
    return true;
  };
  std::size_t spaces = 0;
  for (const auto& glyph : layout.glyphs) if (glyph.justify_gap) ++spaces;
  if (!spaces) return draw_normal();
  const auto space_advance = font.Advance(u' ');
  const std::int64_t content_width = std::int64_t(layout.advance) -
                                     std::int64_t(spaces) * space_advance;
  const double space_width = static_cast<double>(std::int64_t(width) - content_width) /
                             static_cast<double>(spaces);
  if (space_width < 2.0) return draw_normal();
  std::size_t seen = 0;
  for (const auto& glyph : layout.glyphs) {
    const double position = static_cast<double>(x) + glyph.x +
                            static_cast<double>(seen) * (space_width - space_advance);
    std::int32_t at{};
    if (position < INT32_MIN || position > INT32_MAX ||
        !Narrow(static_cast<std::int64_t>(position), &at))
      return Fail(error, "justified glyph position overflow");
    if (!font_renderer::DrawGlyph(font, glyph.code, target, at, baseline_y,
                                  use_tagged_color ? glyph.color : solid_color, error)) return false;
    if (glyph.justify_gap) ++seen;
  }
  return true;
}

bool DrawJustifiedTaggedText16(const aft_font::AftFont& font, std::u16string_view text,
                               const Options& options, font_renderer::Target target,
                               std::int32_t x, std::int32_t baseline_y, std::int32_t width,
                               std::string* error) {
  Layout layout;
  return LayoutLine(font, text, options, &layout, error) &&
         DrawJustifiedLayout(font, layout, target, x, baseline_y, width,
                             true, options.default_color, error);
}

}  // namespace srhd_awa::platform::tagged_text

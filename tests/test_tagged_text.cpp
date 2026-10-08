#include "aft_font.hpp"
#include "m23_aft_fixture.hpp"
#include "tagged_text.hpp"

#include <array>
#include <cassert>
#include <cstdio>

using namespace srhd_awa::platform;
int main() {
  const auto bytes = m23_test::AftFixture();
  aft_font::AftFont font;
  assert(font.Load(bytes.data(), bytes.size()));
  std::vector<tagged_text::Token> tokens;
  std::string error;
  assert(tagged_text::Tokenize(u"ABC", &tokens, &error) && tokens.size() == 3);
  assert(tagged_text::Tokenize(u"<", &tokens, &error) && tokens.size() == 1 &&
         tokens[0].kind == tagged_text::TokenKind::Glyph);
  assert(tagged_text::Tokenize(u"<<fix>A", &tokens, &error) && tokens.size() == 3 &&
         tokens[0].kind == tagged_text::TokenKind::Glyph &&
         tokens[1].kind == tagged_text::TokenKind::FixStart);
  assert(!tagged_text::Tokenize(u"<unsupported>A", &tokens, &error) && !error.empty());
  tagged_text::Layout layout;
  assert(tagged_text::LayoutLine(font, u"", {}, &layout, &error));
  assert(layout.glyphs.empty() && layout.bounds.left == 0 && layout.bounds.right == 0);
  assert(tagged_text::LayoutLine(font, u"<color=255,0,0>A<color=0,255,0>B</color>C</color>", {}, &layout, &error));
  assert(layout.glyphs.size() == 3 && layout.glyphs[0].color == 0xf800 &&
         layout.glyphs[1].color == 0x07e0 && layout.glyphs[2].color == 0xf800);
  assert(tagged_text::LayoutLine(font, u"</color>A<color=0,0,255>B", {}, &layout, &error));
  assert(layout.glyphs[0].color == 0xffff && layout.glyphs[1].color == 0x001f);
  tagged_text::Options shadow{}; shadow.default_color = 0x39e7; shadow.color_tags_enabled = false;
  assert(tagged_text::LayoutLine(font, u"<color=255,0,0>A", shadow, &layout, &error));
  assert(layout.glyphs[0].color == 0x39e7);
  assert(tagged_text::LayoutLine(font, u"<fix>A<fix>B</fix>C</fix>A", {}, &layout, &error));
  assert(layout.glyphs.size() == 4 && layout.glyphs[0].x == 0 &&
         layout.glyphs[1].x == 6 && layout.glyphs[2].x == 12 && layout.glyphs[3].x == 18);
  assert(tagged_text::LayoutLine(font, u"<format=right,4>A</format>", {}, &layout, &error));
  assert(layout.glyphs.size() == 1 && layout.glyphs[0].x == 12 && layout.advance == 17);
  assert(tagged_text::LayoutLine(font, u"<format=center,4>A</format>", {}, &layout, &error));
  assert(layout.glyphs[0].x == 4 && layout.advance == 17);
  tagged_text::Bounds bounds{};
  assert(tagged_text::MeasureTaggedTextBounds(font, u"<format=right,4>A</format>", &bounds));
  assert(bounds.right == 17 && bounds.left == 0);
  assert(tagged_text::LayoutLine(font, u"<td=20>A", {}, &layout, &error));
  assert(layout.glyphs[0].x == 20 && layout.advance == 25);
  assert(tagged_text::LayoutLine(font, u"<align=right>AB</align>", {}, &layout, &error));
  assert(layout.glyphs[0].x == -10 && layout.glyphs[1].x == -5 && layout.advance == 0);
  assert(!tagged_text::LayoutLine(font, u"<align=RIGHT>A</align>", {}, &layout, &error));
  assert(tagged_text::LayoutLine(font, u"<object=7,9,6,0>A", {}, &layout, &error));
  assert(layout.objects.size() == 1 && layout.objects[0].id == 7 &&
         layout.objects[0].width == 9 && layout.objects[0].height == 6 &&
         layout.objects[0].x == 0 && layout.glyphs[0].x == 9);
  std::vector<std::u16string> lines;
  assert(tagged_text::WrapTaggedTextIntoLines(font, u"A B C", 9, &lines, &error));
  assert(lines.size() >= 2);
  assert(tagged_text::WrapTaggedTextIntoLines(font, u"ABC", 2, &lines, &error));
  assert(!lines.empty() && lines.size() <= 3);
  assert(tagged_text::WrapTaggedTextIntoLines(font, u"<color=255,0,0>AB</color>", 5,
                                             &lines, &error));
  assert(lines.size() == 2 && lines[0] == u"<color=255,0,0>A" &&
         lines[1] == u"B</color>");
  std::array<std::uint16_t, 32 * 8> pixels{};
  font_renderer::Target target{pixels.data(), 32, 8, 32, {0, 0, 32, 8}};
  assert(tagged_text::DrawJustifiedTaggedText16(font, u"A B", {}, target, 0, 1, 24, &error));
  assert(pixels[32 + 19] != 0 && pixels[32 + 9] == 0);
  pixels.fill(0);
  assert(tagged_text::DrawJustifiedTaggedText16(font, u"A B C", {}, target, 0, 1, 31, &error));
  assert(pixels[32 + 13] != 0 && pixels[32 + 26] != 0 && pixels[32 + 9] == 0);
  pixels.fill(0);
  assert(tagged_text::DrawJustifiedTaggedText16(font, u" A B", {}, target, 0, 1, 24, &error));
  assert(pixels[32 + 19] != 0 && pixels[32 + 9] == 0);
  pixels.fill(0);
  assert(tagged_text::DrawJustifiedTaggedText16(font, u"A B", {}, target, 0, 1, 11, &error));
  assert(pixels[32 + 9] != 0 && pixels[32 + 19] == 0);
  pixels.fill(0);
  assert(tagged_text::DrawJustifiedTaggedText16(font, u"AB", {}, target, 0, 1, 24, &error));
  assert(pixels[32 + 5] != 0 && pixels[32 + 19] == 0);
  std::puts("M23 tagged text PASS");
}

#include "aft_font.hpp"
#include "font_renderer.hpp"
#include "m23_aft_fixture.hpp"

#include <array>
#include <cassert>
#include <cstdio>

using namespace srhd_awa::platform;
int main() {
  const auto bytes = m23_test::AftFixture();
  aft_font::AftFont font;
  assert(font.Load(bytes.data(), bytes.size()));
  std::array<std::uint16_t, 10 * 8> pixels{};
  const font_renderer::Target target{pixels.data(), 10, 8, 10, {0, 0, 10, 8}};
  assert(font_renderer::DrawGlyph(font, u'A', target, 1, 1, 0xf800));
  for (int y = 1; y < 3; ++y) for (int x = 1; x < 3; ++x)
    assert(pixels[y * 10 + x] == 0xf800);
  assert(font_renderer::DrawGlyph(font, u'B', target, 4, 1, 0x07e0));
  assert(pixels[1 * 10 + 4] != 0 && pixels[1 * 10 + 4] != 0x07e0);
  assert(font_renderer::DrawGlyph(font, u'C', target, 7, 1, 0x001f));
  assert(pixels[1 * 10 + 7] != 0);
  const auto first = pixels;
  pixels.fill(0);
  assert(font_renderer::DrawGlyph(font, u'A', target, 1, 1, 0xf800));
  assert(font_renderer::DrawGlyph(font, u'B', target, 4, 1, 0x07e0));
  assert(font_renderer::DrawGlyph(font, u'C', target, 7, 1, 0x001f));
  assert(pixels == first);
  pixels.fill(0);
  const font_renderer::Target clipped{pixels.data(), 10, 8, 10, {1, 1, 4, 4}};
  assert(font_renderer::DrawGlyph(font, u'я', clipped, 1, 1, 0xffff));
  for (int y = 0; y < 8; ++y) for (int x = 0; x < 10; ++x)
    if (x < 1 || x >= 4 || y < 1 || y >= 4) assert(pixels[y * 10 + x] == 0);
  pixels.fill(0);
  assert(font_renderer::DrawGlyph(font, u'Ё', target, 0, 0, 0xffff));
  assert(font_renderer::DrawGlyph(font, u'Z', target, 0, 0, 0xffff));
  std::puts("M23 RGB565 font renderer PASS");
}

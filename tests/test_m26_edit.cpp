#include "m23_aft_fixture.hpp"
#include "ui_edit.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform;
void Check(bool value, const char* reason) {
  if (!value) { std::fprintf(stderr, "M26 EDIT FAIL: %s\n", reason); std::exit(1); }
}
}

int main() {
  const auto bytes = m23_test::AftFixture();
  auto font = std::make_shared<aft_font::AftFont>();
  Check(font->Load(bytes.data(), bytes.size()), "synthetic AFT fixture");
  ui::UiEdit edit;
  edit.SetSize({20, 8});
  edit.SetFont("fixture", font);
  Check(edit.TextColor() == 0xffff && edit.CaretColor() == 0xf800 &&
        !edit.BorderEnabled() && edit.BorderLight() == 0xffff &&
        edit.BorderDark() == 0x31a6 && edit.MaxLength() == 256 &&
        edit.CaretPosition() == 0 && !edit.AutoScrollText() &&
        edit.AlignX() == ui::EditAlignX::Left && edit.ClearFocusOnEnter(),
        "upstream Edit defaults");
  edit.SetText(u"ABC");
  Check(edit.TextStart() == ui::Point{2, 5}, "left text placement and baseline");
  std::array<std::uint16_t, 20 * 8> pixels{};
  scene_compositor::Framebuffer target{pixels.data(), 20, 8, 20};
  Check(edit.RenderLeaf(target, {0, 0, 20, 8}), "unfocused text render");
  const auto plain = pixels;
  edit.SetFocused(true);
  edit.SetCaretBlink(true);
  pixels.fill(0);
  Check(edit.RenderLeaf(target, {0, 0, 20, 8}) && pixels != plain,
        "focused caret-on frame differs");
  const auto caret_x = edit.TextStart().x + font->Advance(u'A') +
      font->Advance(u'B') + font->Advance(u'C');
  Check(caret_x >= 0 && caret_x + 1 < 20 &&
        pixels[static_cast<std::size_t>(caret_x) + 20 * 4] == 0xf800 &&
        pixels[static_cast<std::size_t>(caret_x + 1) + 20 * 4] == 0xf800,
        "two RGB565 caret lines");
  edit.SetCaretBlink(false);
  pixels.fill(0);
  Check(edit.RenderLeaf(target, {0, 0, 20, 8}) && pixels == plain,
        "focused caret-off frame matches unfocused pixels");
  edit.SetBorder(true, 0xffff, 0x001f);
  pixels.fill(0);
  Check(edit.RenderLeaf(target, {0, 0, 20, 8}) &&
        pixels[0] == 0xffff && pixels[19] == 0x001f &&
        pixels[7 * 20] == 0xffff && pixels[7 * 20 + 19] == 0x001f,
        "light top-left and dark bottom-right border");
  edit.SetAlignX(ui::EditAlignX::Center);
  Check(edit.TextStart().x != 2, "centered text placement");
  edit.SetMaxLength(3);
  Check(!edit.InsertCharacter(u'A'), "MaxLen enforced");
  edit.Backspace();
  Check(edit.Text() == u"AB" && edit.CaretPosition() == 2 &&
        edit.InsertCharacter(u'A') && edit.Text() == u"ABA",
        "editing state methods");
  edit.Home(); edit.Delete();
  Check(edit.Text() == u"BA" && edit.CaretPosition() == 0,
        "Home and Delete");
  edit.MoveRight(); edit.MoveLeft();
  Check(edit.CaretPosition() == 0, "caret movement");
  edit.SetText(u"AAAAAAAAAAAA");
  edit.SetAutoScrollText(true);
  edit.SetFocused(false);
  edit.SetFocused(true);
  edit.SetAlignX(ui::EditAlignX::Left);
  pixels.fill(0);
  Check(edit.RenderLeaf(target, {0, 0, 20, 8}) &&
        edit.FirstCharacter() > 0, "long text scrolls toward caret");
  std::puts("M26 EDIT CPU PASS");
}

#include "aft_font.hpp"
#include "gi_object.hpp"
#include "image_object.hpp"
#include "m23_aft_fixture.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_label.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using srhd_awa::package::Package;
using srhd_awa::platform::aft_font::AftFont;
using srhd_awa::platform::image_object::Kind;
using srhd_awa::platform::scene_compositor::Framebuffer;
using srhd_awa::platform::ui::LabelAlignX;
using srhd_awa::platform::ui::LabelAlignY;
using srhd_awa::platform::ui::Point;
using srhd_awa::platform::ui::Size;
using srhd_awa::platform::ui::UiLabelLeaf;
using srhd_awa::platform::ui::UiObject;
using srhd_awa::platform::ui::UiTree;
using srhd_awa::platform::ui_fingerprint::Value;

void Check(bool value, const char* what) {
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
void U32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i != 4; ++i) (*bytes)[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::vector<std::uint8_t> Bmp(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  std::vector<std::uint8_t> bytes(58); bytes[0] = 'B'; bytes[1] = 'M';
  U32(&bytes, 2, bytes.size()); U32(&bytes, 10, 54); U32(&bytes, 14, 40);
  U32(&bytes, 18, 1); U32(&bytes, 22, 1); bytes[26] = 1; bytes[28] = 24;
  U32(&bytes, 34, 4); bytes[54] = blue; bytes[55] = green; bytes[56] = red;
  return bytes;
}
std::vector<std::uint8_t> Gi(std::uint16_t color) {
  std::vector<std::uint8_t> bytes(104); std::memcpy(bytes.data(), "gi\0", 3);
  U32(&bytes, 4, 1); U32(&bytes, 16, 2); U32(&bytes, 20, 2);
  U32(&bytes, 24, 0xf800); U32(&bytes, 28, 0x07e0); U32(&bytes, 32, 0x001f);
  U32(&bytes, 44, 1); U32(&bytes, 64, 96); U32(&bytes, 68, 8);
  U32(&bytes, 80, 2); U32(&bytes, 84, 2);
  for (std::size_t at = 96; at < 104; at += 2) {
    bytes[at] = static_cast<std::uint8_t>(color);
    bytes[at + 1] = static_cast<std::uint8_t>(color >> 8);
  }
  return bytes;
}
std::vector<std::uint8_t> Gai() {
  const auto first = Gi(0xf800), second = Gi(0x07e0);
  constexpr std::size_t directory = 64;
  const auto second_at = directory + first.size(), table = second_at + second.size();
  std::vector<std::uint8_t> bytes(table + 36); std::memcpy(bytes.data(), "gai\0", 4);
  U32(&bytes, 4, 1); U32(&bytes, 16, 2); U32(&bytes, 20, 2);
  U32(&bytes, 24, 2); U32(&bytes, 32, table); U32(&bytes, 36, 36);
  U32(&bytes, 48, directory); U32(&bytes, 52, first.size());
  U32(&bytes, 56, second_at); U32(&bytes, 60, second.size());
  std::memcpy(bytes.data() + directory, first.data(), first.size());
  std::memcpy(bytes.data() + second_at, second.data(), second.size());
  U32(&bytes, table, 1); U32(&bytes, table + 8, 16); U32(&bytes, table + 16, 2);
  U32(&bytes, table + 20, 0); U32(&bytes, table + 24, 10);
  U32(&bytes, table + 28, 1); U32(&bytes, table + 32, 10);
  return bytes;
}
void Entry(std::vector<std::uint8_t>* bytes, std::size_t at, const char* name,
           std::uint32_t target, std::uint32_t size) {
  std::memset(bytes->data() + at, 0, 158); U32(bytes, at + 4, size);
  std::strncpy(reinterpret_cast<char*>(bytes->data() + at + 8), name, 62);
  std::strncpy(reinterpret_cast<char*>(bytes->data() + at + 71), name, 62);
  U32(bytes, at + 150, target);
}
bool WritePackage(const char* path) {
  const std::vector<std::pair<const char*, std::vector<std::uint8_t>>> items = {
      {"B.BMP", Bmp(0, 0, 255)}, {"R.BMP", Bmp(255, 0, 0)}, {"GI.GAI", Gai()}};
  constexpr std::size_t root = 4, directory = 12 + 3 * 158;
  std::size_t cursor = root + directory; std::vector<std::uint8_t> bytes(cursor);
  U32(&bytes, 0, root); U32(&bytes, root, directory); U32(&bytes, root + 4, items.size());
  U32(&bytes, root + 8, 158);
  for (std::size_t i = 0; i < items.size(); ++i) {
    const auto target = cursor; bytes.resize(cursor + 4 + items[i].second.size());
    U32(&bytes, cursor, items[i].second.size());
    std::memcpy(bytes.data() + cursor + 4, items[i].second.data(), items[i].second.size());
    Entry(&bytes, root + 12 + i * 158, items[i].first,
          static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(items[i].second.size()));
    cursor = bytes.size();
  }
  FILE* file = std::fopen(path, "wb"); if (!file) return false;
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  std::fclose(file); return ok;
}
void AddImage(UiObject* parent, Package* package, const char* resource,
              Point position, Size size, double depth, const char* name) {
  auto* leaf = parent->AddImage(); leaf->SetName(name); leaf->Image().SetPackage(package);
  leaf->SetPosition(position); leaf->SetSize(size); leaf->SetDepth(depth);
  leaf->Image().SetModes(srhd_awa::platform::image_layout::XMode::LeftFill,
                         srhd_awa::platform::image_layout::YMode::TopFill);
  std::string error; Check(leaf->Load(Kind::Simple, resource, "", &error), error.c_str());
}
UiLabelLeaf* AddLabel(UiObject* parent, const char* name,
                      std::shared_ptr<const AftFont> font, Point position,
                      Size size, double depth, std::u16string text, std::uint16_t color) {
  auto* label = parent->AddLabel(); label->SetName(name); label->SetPosition(position);
  label->SetSize(size); label->SetDepth(depth); label->SetFont("fixture", std::move(font));
  label->SetTextLines({std::move(text)}); label->SetTextColor(color);
  label->SetAlignX(LabelAlignX::Left); label->SetAlignY(LabelAlignY::Top);
  std::string error; Check(label->Prepare(&error), error.c_str()); return label;
}
void Put32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned i = 0; i != 4; ++i) bytes->push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void PutText(std::vector<std::uint8_t>* bytes, const std::string& name,
             const UiLabelLeaf& label) {
  Put32(bytes, name.size()); bytes->insert(bytes->end(), name.begin(), name.end());
  const auto& text = label.TextLines().front(); Put32(bytes, text.size() * 2);
  for (char16_t code : text) {
    bytes->push_back(static_cast<std::uint8_t>(code));
    bytes->push_back(static_cast<std::uint8_t>(code >> 8));
  }
  const auto position = label.AbsolutePosition();
  const auto bounds = label.ContentBounds();
  const auto size = label.ContentSize();
  for (const auto value : {position.x, position.y, bounds.left, bounds.top,
                           bounds.right, bounds.bottom, size.width, size.height})
    Put32(bytes, static_cast<std::uint32_t>(value));
  Put32(bytes, label.RenderedLineCount());
}
Value Hash(const std::uint8_t* bytes, std::size_t count) {
  Value result{0xffffffffu, 0xcbf29ce484222325ull, count};
  for (std::size_t i = 0; i < count; ++i) {
    result.crc32 ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      result.crc32 = (result.crc32 >> 1) ^ (0xedb88320u & (0u - (result.crc32 & 1u)));
    result.fnv64 = (result.fnv64 ^ bytes[i]) * 0x100000001b3ull;
  }
  result.crc32 ^= 0xffffffffu; return result;
}
void CheckValue(Value actual, std::uint32_t crc, std::uint64_t fnv, const char* name) {
  if (actual.crc32 != crc || actual.fnv64 != fnv) {
    std::fprintf(stderr, "FAIL %s: actual=%08x/%016llx bytes=%zu expected=%08x/%016llx\n",
                 name, actual.crc32, static_cast<unsigned long long>(actual.fnv64),
                 actual.bytes, crc, static_cast<unsigned long long>(fnv));
    std::exit(1);
  }
}
Value LayoutHash(const UiLabelLeaf& first, const UiLabelLeaf& second) {
  std::vector<std::uint8_t> bytes;
  PutText(&bytes, "scroll-label", first); PutText(&bytes, "nested-label", second);
  return Hash(bytes.data(), bytes.size());
}
void CheckRealLabel(const std::string& game_root) {
  Package package; std::string error;
  Check(package.Open(game_root + "/DATA/forms.pkg", &error), error.c_str());
  const auto* entry = package.Resolve("DATA/FONT/Verdana_11_3.aft");
  Check(entry != nullptr, "real Font.2Intro backing entry");
  std::vector<std::uint8_t> source;
  Check(package.ReadPayload(*entry, &source, &error), error.c_str());
  const auto source_hash = Hash(source.data(), source.size());
  Check(source.size() == 26669, "real Font.2Intro source size");
  CheckValue(source_hash, 0x93df743fu, 0x4c320b6bc6048343ull, "Python real font source oracle");
  auto font = std::make_shared<AftFont>();
  Check(font->Load(source.data(), source.size(), &error), error.c_str());
  const auto fingerprint = font->fingerprint();
  Check(fingerprint.crc32 == 0x7ecfe087u && fingerprint.fnv64 == 0x1a1527c347b838c2ull,
        "Python real AFT structural oracle");
  Check(font->glyphs().size() == 214 && font->line_height() == 16 &&
        font->centering_height() == 11 && font->above_baseline() == 16 &&
        font->below_baseline() == 2 && font->max_glyph_advance() == 16,
        "Python real AFT metrics oracle");
  for (const std::u16string_view text : {u"Привет", u"Космические рейнджеры",
                                       u"Ёжик", u"Торговый центр"}) {
    for (const auto code : text) Check(font->Find(code) != nullptr, "real Cyrillic corpus glyph");
    srhd_awa::platform::tagged_text::Bounds corpus_bounds{};
    Check(srhd_awa::platform::tagged_text::MeasureTaggedTextBounds(*font, text,
                                                                   &corpus_bounds, nullptr, &error),
          error.c_str());
    Check(corpus_bounds.right > corpus_bounds.left, "real Cyrillic corpus measure");
  }
  UiTree tree; auto* root = tree.Root(); root->SetName("m23-root"); root->SetSize({1024, 60});
  auto* label = AddLabel(root, "WinText", font, {0, 10}, {1024, 40}, 8,
                         u"\u0412 \u044b  \u043f \u043e \u0431 \u0435 \u0434 \u0438 \u043b \u0438 !",
                         srhd_awa::platform::tagged_text::PackRgb565(255, 255, 230));
  label->SetFont("Font.2Intro", font);
  label->SetAlignX(LabelAlignX::Center);
  label->SetAlignY(LabelAlignY::CenterEx);
  label->SetTextShadow(1, srhd_awa::platform::tagged_text::PackRgb565(0, 0, 255));
  Check(label->Prepare(&error), error.c_str());
  const auto bounds = label->ContentBounds();
  const auto size = label->ContentSize();
  Check(bounds.left == 1 && bounds.top == -12 && bounds.right == 156 && bounds.bottom == 2 &&
        size.width == 156 && size.height == 17 && label->RenderedLineCount() == 1,
        "Python real Label bounds and line count oracle");
  std::vector<std::uint16_t> pixels(1024 * 60, 0);
  Framebuffer target{pixels.data(), 1024, 60, 1024};
  Check(tree.Render(target, &error), error.c_str());
  Value frame{}, tree_value{};
  Check(srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(target, &frame, &error), error.c_str());
  Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &tree_value, &error), error.c_str());
  CheckValue(tree_value, 0xef3ef436u, 0x6022c76fb3cb9306ull, "Python real Label tree oracle");
  if (frame.crc32 != 0xb36cfe2fu || frame.fnv64 != 0x6b916c3b29d2a194ull) {
    std::size_t changed = 0, shown = 0;
    int left = 1024, top = 60, right = 0, bottom = 0;
    for (int y = 0; y < 60; ++y) for (int x = 0; x < 1024; ++x) {
      const auto color = pixels[y * 1024 + x]; if (!color) continue;
      ++changed; left = std::min(left, x); top = std::min(top, y);
      right = std::max(right, x + 1); bottom = std::max(bottom, y + 1);
      if (shown++ < 16) std::fprintf(stderr, "real pixel (%d,%d)=%04x\n", x, y, color);
    }
    std::fprintf(stderr, "real changed=%zu pixel_bounds=%d,%d,%d,%d\n",
                 changed, left, top, right, bottom);
  }
  CheckValue(frame, 0xb36cfe2fu, 0x6b916c3b29d2a194ull, "Python real Label RGB565 oracle");
  std::puts("M23 REAL LABEL ORACLE PASS");
}
}  // namespace

int main(int argc, char** argv) {
  constexpr const char* path = "build/test_m23_ui_text.pkg";
  Check(WritePackage(path), "write synthetic package");
  Package package; std::string error; Check(package.Open(path, &error), error.c_str());
  const auto source = m23_test::AftFixture();
  auto font = std::make_shared<AftFont>();
  Check(font->Load(source.data(), source.size(), &error), error.c_str());
  const auto source_hash = Hash(source.data(), source.size());
  CheckValue(source_hash, 0x6b655a7fu, 0xf7cedbbab2168e77ull, "Python font source oracle");
  const auto structure = font->fingerprint();
  Check(structure.crc32 == 0x2f1c1442u && structure.fnv64 == 0xfaff5b46e2b3d02eull,
        "Python AFT structure oracle");

  UiTree tree; auto* root = tree.Root(); root->SetName("root"); root->SetSize({12, 10});
  AddImage(root, &package, "B.BMP", {0, 0}, {12, 10}, 100, "background");
  auto* panel = root->AddPanel(); panel->SetName("panel"); panel->SetPosition({1, 1});
  panel->SetSize({10, 8}); panel->SetDepth(10);
  AddImage(panel, &package, "R.BMP", {0, 0}, {10, 8}, 100, "image");
  auto* first = AddLabel(panel, "scroll-label", font, {0, 0}, {8, 7}, 5, u"A", 0xffff);
  first->SetPositionModeW(true);
  auto* nested = panel->AddPanel(); nested->SetName("nested"); nested->SetPosition({3, 2});
  nested->SetOrigin({1, 0}); nested->SetSize({6, 5});
  auto* gi = nested->AddGIObject(); gi->SetName("gi"); gi->Image().SetPackage(&package);
  gi->SetPosition({3, 3}); gi->SetDepth(5); Check(gi->LoadResource("GI.GAI", &error), error.c_str());
  auto* second = AddLabel(nested, "nested-label", font, {0, 0}, {6, 5}, 0, u"A", 0x07e0);
  auto* inactive = AddLabel(panel, "inactive", font, {0, 0}, {10, 8}, -10, u"B", 0);
  inactive->SetActive(false);
  inactive->SetTextLines({u"A", u"B"}); // Dirty; traversal must not remeasure it.

  std::vector<std::uint16_t> pixels(120, 0);
  Framebuffer target{pixels.data(), 12, 10, 12};
  Check(tree.Render(target, &error), error.c_str());
  Value tree_a{}, frame_a{};
  Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &tree_a, &error), error.c_str());
  Check(srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(target, &frame_a, &error), error.c_str());
  CheckValue(LayoutHash(*first, *second), 0x00af5933u, 0x6fc00b179d894698ull, "Python layout A oracle");
  CheckValue(tree_a, 0x19bf1b94u, 0x3a3e48a85aedf03bull, "Python tree A oracle");
  CheckValue(frame_a, 0xe227128cu, 0xc27811b5ad7e7fddull, "Python frame A oracle");

  first->SetTextLines({u"AA"}); panel->SetScrollOffset({1, 0});
  Check(tree.Update(10, &error), error.c_str());
  pixels.assign(120, 0); Check(tree.Render(target, &error), error.c_str());
  Value tree_b{}, frame_b{};
  Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &tree_b, &error), error.c_str());
  Check(srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(target, &frame_b, &error), error.c_str());
  CheckValue(LayoutHash(*first, *second), 0x35806d66u, 0x0b66e23e077a1dd2ull, "Python layout B oracle");
  CheckValue(tree_b, 0xadbefa24u, 0xb446e66bca28b6f4ull, "Python tree B oracle");
  CheckValue(frame_b, 0x3793178eu, 0x38c61f5d7d590181ull, "Python frame B oracle");
  Check(first->AbsolutePosition() == Point{0, 1} && inactive->RenderedLineCount() == 1,
        "PositionModeW scroll and inactive traversal");
  srhd_awa::platform::image_object::PortableImageObject background(&package);
  background.SetModes(srhd_awa::platform::image_layout::XMode::LeftFill,
                      srhd_awa::platform::image_layout::YMode::TopFill);
  Check(background.Load(Kind::Simple, "R.BMP", "", &error), error.c_str());
  srhd_awa::platform::ui::UiTree background_tree;
  background_tree.SetRootSize({8, 8});
  auto* backed = AddLabel(background_tree.Root(), "backed", font, {0, 0}, {8, 8},
                          0, u"A", 0xffff);
  backed->SetBackground(std::move(background));
  backed->SetTextShadow(1, 0x001f);
  std::vector<std::uint16_t> background_pixels(64, 0);
  Framebuffer background_target{background_pixels.data(), 8, 8, 8};
  Check(background_tree.Render(background_target, &error), error.c_str());
  Check(background_pixels[6 * 8 + 6] == 0xf800 &&
        background_pixels[5 * 8 + 4] == 0x001f &&
        background_pixels[3 * 8 + 2] == 0xffff,
        "Label embedded M21 background precedes shadow and main text");
  if (argc > 1) CheckRealLabel(argv[1]);
  std::remove(path); std::puts("M23 MIXED UI TEXT ORACLE PASS");
}

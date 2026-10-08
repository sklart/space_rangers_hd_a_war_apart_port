#include "gi_object.hpp"
#include "image_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_window.hpp"
#include "ui_graph_button.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {
using srhd_awa::package::Package;
using srhd_awa::platform::image_object::Kind;
using srhd_awa::platform::scene_compositor::Framebuffer;
using srhd_awa::platform::ui::Point;
using srhd_awa::platform::ui::Size;
using srhd_awa::platform::ui::UiGILeaf;
using srhd_awa::platform::ui::UiImageLeaf;
using srhd_awa::platform::ui::UiObject;
using srhd_awa::platform::ui::UiTree;
using srhd_awa::platform::ui_fingerprint::Value;

void U32(std::vector<std::uint8_t>* b, std::size_t at, std::uint32_t value) { for (unsigned i = 0; i < 4; ++i) (*b)[at + i] = static_cast<std::uint8_t>(value >> (i * 8)); }
void BE16(std::vector<std::uint8_t>* b, std::size_t at, std::uint16_t value) { (*b)[at] = static_cast<std::uint8_t>(value >> 8); (*b)[at + 1] = static_cast<std::uint8_t>(value); }
void BE32(std::vector<std::uint8_t>* b, std::size_t at, std::uint32_t value) { for (unsigned i = 0; i < 4; ++i) (*b)[at + i] = static_cast<std::uint8_t>(value >> (24 - i * 8)); }
std::vector<std::uint8_t> Bmp(std::uint8_t red, std::uint8_t green, std::uint8_t blue, bool keyed = false) {
  const int width = keyed ? 2 : 1; std::vector<std::uint8_t> b(54 + 4 * ((width * 3 + 3) / 4)); b[0] = 'B'; b[1] = 'M'; U32(&b, 2, b.size()); U32(&b, 10, 54); U32(&b, 14, 40); U32(&b, 18, width); U32(&b, 22, 1); b[26] = 1; b[28] = 24; U32(&b, 34, b.size() - 54);
  if (keyed) { b[54] = 0; b[55] = 0; b[56] = 0; b[57] = blue; b[58] = green; b[59] = red; } else { b[54] = blue; b[55] = green; b[56] = red; } return b;
}
std::vector<std::uint8_t> Psd() { std::vector<std::uint8_t> b(44); std::memcpy(b.data(), "8BPS", 4); BE16(&b, 4, 1); BE16(&b, 12, 4); BE32(&b, 14, 1); BE32(&b, 18, 1); BE16(&b, 22, 8); BE16(&b, 24, 3); b[40] = 255; b[43] = 128; return b; }
std::vector<std::uint8_t> Gi(std::uint16_t color) {
  std::vector<std::uint8_t> b(104); std::memcpy(b.data(), "gi\0", 3); U32(&b, 4, 1); U32(&b, 16, 2); U32(&b, 20, 2); U32(&b, 24, 0xf800); U32(&b, 28, 0x07e0); U32(&b, 32, 0x001f); U32(&b, 44, 1); U32(&b, 64, 96); U32(&b, 68, 8); U32(&b, 80, 2); U32(&b, 84, 2); for (std::size_t at = 96; at < 104; at += 2) { b[at] = static_cast<std::uint8_t>(color); b[at + 1] = static_cast<std::uint8_t>(color >> 8); } return b;
}
std::vector<std::uint8_t> Gai() {
  const auto first = Gi(0xf800), second = Gi(0x07e0); constexpr std::size_t directory = 64; const auto second_at = directory + first.size(), table = second_at + second.size(); std::vector<std::uint8_t> b(table + 36); std::memcpy(b.data(), "gai\0", 4); U32(&b, 4, 1); U32(&b, 16, 2); U32(&b, 20, 2); U32(&b, 24, 2); U32(&b, 32, table); U32(&b, 36, 36); U32(&b, 48, directory); U32(&b, 52, first.size()); U32(&b, 56, second_at); U32(&b, 60, second.size()); std::memcpy(b.data() + directory, first.data(), first.size()); std::memcpy(b.data() + second_at, second.data(), second.size()); U32(&b, table, 1); U32(&b, table + 8, 16); U32(&b, table + 16, 2); U32(&b, table + 20, 0); U32(&b, table + 24, 10); U32(&b, table + 28, 1); U32(&b, table + 32, 10); return b;
}
void Entry(std::vector<std::uint8_t>* b, std::size_t at, const char* name, std::uint32_t target, std::uint32_t size) { std::memset(b->data() + at, 0, 158); U32(b, at + 4, size); std::strncpy(reinterpret_cast<char*>(b->data() + at + 8), name, 62); std::strncpy(reinterpret_cast<char*>(b->data() + at + 71), name, 62); U32(b, at + 150, target); }
bool WritePackage(const char* path) {
  const std::vector<std::pair<const char*, std::vector<std::uint8_t>>> items = {{"GI.GAI", Gai()}, {"S.BMP", Bmp(0, 0, 255)}, {"T.BMP", Bmp(0, 255, 0, true)}, {"A.PSD", Psd()}, {"I.BMP", Bmp(255, 0, 0)}}; constexpr std::size_t root = 4, directory = 12 + 5 * 158; std::size_t cursor = root + directory; std::vector<std::uint8_t> b(cursor); U32(&b, 0, root); U32(&b, root, directory); U32(&b, root + 4, items.size()); U32(&b, root + 8, 158);
  for (std::size_t i = 0; i < items.size(); ++i) { const auto target = cursor; b.resize(cursor + 4 + items[i].second.size()); U32(&b, cursor, items[i].second.size()); std::memcpy(b.data() + cursor + 4, items[i].second.data(), items[i].second.size()); Entry(&b, root + 12 + i * 158, items[i].first, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(items[i].second.size())); cursor = b.size(); }
  FILE* file = std::fopen(path, "wb"); if (!file) return false; const bool ok = std::fwrite(b.data(), 1, b.size(), file) == b.size(); std::fclose(file); return ok;
}
void Check(bool value, const char* what) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); } }
UiImageLeaf* AddImage(UiObject* parent, Package* package, Kind kind, const char* resource, Point position, Size size, double depth, const char* name, bool active = true) {
  auto* result = parent->AddImage(); result->SetName(name); result->Image().SetPackage(package); result->SetPosition(position); result->SetSize(size); result->SetDepth(depth); result->SetActive(active); result->Image().SetModes(srhd_awa::platform::image_layout::XMode::LeftFill, srhd_awa::platform::image_layout::YMode::TopFill); std::string error; Check(result->Load(kind, resource, "", &error), error.c_str()); return result;
}
UiGILeaf* AddGi(UiObject* parent, Package* package, Point position, double depth, const char* name) { auto* result = parent->AddGIObject(); result->SetName(name); result->Image().SetPackage(package); result->SetPosition(position); result->SetDepth(depth); std::string error; Check(result->LoadResource("GI.GAI", &error), error.c_str()); return result; }
void CheckValue(const Value& actual, std::uint32_t crc, std::uint64_t fnv, std::size_t bytes, const char* what) { if (actual.crc32 != crc || actual.fnv64 != fnv || actual.bytes != bytes) { std::fprintf(stderr, "FAIL: %s got crc=%08x fnv=%016llx bytes=%zu\n", what, actual.crc32, static_cast<unsigned long long>(actual.fnv64), actual.bytes); std::exit(1); } }
}  // namespace

int main() {
  constexpr const char* path = "build/test_ui_tree.pkg"; Check(WritePackage(path), "write package"); Package package; std::string error; Check(package.Open(path, &error), error.c_str()); UiTree tree; auto* root = tree.Root(); root->SetName("root"); root->SetSize({6, 4});
  AddImage(root, &package, Kind::Simple, "S.BMP", {0, 0}, {6, 4}, 100, "background");
  auto* panel_a = root->AddPanel(); panel_a->SetName("panel-a"); panel_a->SetPosition({1, 0}); panel_a->SetOrigin({1, 0}); panel_a->SetSize({4, 4}); panel_a->SetDepth(10);
  auto* panel_b = panel_a->AddPanel(); panel_b->SetName("panel-b"); panel_b->SetPosition({1, 1}); panel_b->SetOrigin({1, 0}); panel_b->SetSize({3, 2}); panel_b->SetDepth(0);
  auto* gi = AddGi(panel_b, &package, {0, 1}, 5, "gi"); AddImage(panel_b, &package, Kind::Trans, "T.BMP", {-1, 0}, {2, 1}, 5, "trans");
  auto* alpha = AddImage(panel_b, &package, Kind::Alpha, "A.PSD", {0, 0}, {1, 1}, 0, "alpha"); alpha->SetOrigin({1, 0}); alpha->SetPositionModeW(true);
  AddImage(root, &package, Kind::Simple, "I.BMP", {0, 0}, {6, 4}, -100, "inactive", false);
  std::vector<std::uint16_t> pixels(24, 0); Framebuffer framebuffer{pixels.data(), 6, 4, 6}; Value tree_a{}, frame_a{}, tree_b{}, frame_b{};
  Check(tree.Render(framebuffer, &error), error.c_str()); Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &tree_a, &error), error.c_str()); Check(srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(framebuffer, &frame_a, &error), error.c_str());
  const std::vector<std::uint16_t> expected_a = {0x001f,0x001f,0x001f,0x001f,0x001f,0x001f, 0x001f,0x780f,0x07e0,0x001f,0x001f,0x001f, 0x001f,0x001f,0xf800,0xf800,0x001f,0x001f, 0x001f,0x001f,0x001f,0x001f,0x001f,0x001f}; Check(pixels == expected_a, "nested clip/origin/simple/trans/alpha/GI frame A"); CheckValue(tree_a, 0xab1bff76u, 0x6f04e776692c26f3ull, 496, "Python tree A oracle"); CheckValue(frame_a, 0x800cafb4u, 0xe1e077394c109d07ull, 48, "Python frame A oracle");
  panel_b->SetScrollOffset({1, 0}); gi->SetDepth(6); Check(tree.Update(10, &error), error.c_str()); pixels.assign(24, 0); Check(tree.Render(framebuffer, &error), error.c_str()); Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &tree_b, &error), error.c_str()); Check(srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(framebuffer, &frame_b, &error), error.c_str());
  const std::vector<std::uint16_t> expected_b = {0x001f,0x001f,0x001f,0x001f,0x001f,0x001f, 0x001f,0x001f,0x07e0,0x001f,0x001f,0x001f, 0x001f,0x001f,0x07e0,0x07e0,0x001f,0x001f, 0x001f,0x001f,0x001f,0x001f,0x001f,0x001f}; Check(pixels == expected_b && alpha->AbsolutePosition() == Point{1, 1}, "ModeW scroll/GI update/depth frame B"); CheckValue(tree_b, 0xe21f4881u, 0xda9cf654e3bf116aull, 496, "Python tree B oracle"); CheckValue(frame_b, 0x1d127015u, 0xe2076f2b18e93b07ull, 48, "Python frame B oracle");
  UiTree window_tree; window_tree.SetRootSize({8, 6});
  auto* window = window_tree.Root()->AddWindow(); window->SetSize({5, 4});
  window->SetMinimumSize({6, 5}); window->SetWorkSubRect({1, 1, 4, 3});
  for (int i = 0; i < 9; ++i) {
    auto border = std::make_unique<UiImageLeaf>(&package);
    Check(border->Load(Kind::Simple, "S.BMP", "", &error), error.c_str());
    Check(window->AddBorderImage(static_cast<srhd_awa::platform::ui::WindowSlot>(i),
                                 std::move(border), &error), error.c_str());
  }
  Check(window->FinalizeLayout(&error) && window->ClientSize() == Size{6, 5} &&
      window->WorkSubRect() == srhd_awa::platform::ui::Rect{1, 1, 4, 3},
      "Window MinSize alignment and WorkSubRect");
  using srhd_awa::platform::ui::WindowSlot;
  Check(window->BorderImage(WindowSlot::TopRight)->LocalPosition() == Point{5, 0} &&
      window->BorderImage(WindowSlot::BottomLeft)->LocalPosition() == Point{0, 4} &&
      window->BorderImage(WindowSlot::Top)->ClientSize() == Size{4, 1} &&
      window->BorderImage(WindowSlot::Left)->ClientSize() == Size{1, 3} &&
      window->BorderImage(WindowSlot::Texture)->ClientSize() == Size{4, 3},
      "Window corner edge and texture layout");
  AddImage(window, &package, Kind::Simple, "I.BMP", {0, 0}, {1, 1}, 0, "over-border");
  std::vector<std::uint16_t> window_pixels(48, 0);
  Framebuffer window_frame{window_pixels.data(), 8, 6, 8};
  Check(window_tree.Render(window_frame, &error) && window_pixels[0] == 0xf800 &&
      window_pixels[1] == 0x001f, "Window child draws over high-depth border");
  UiTree button_tree; button_tree.SetRootSize({5, 3});
  auto* button = button_tree.Root()->AddGraphButton();
  button->SetPosition({1, 1}); button->SetSize({2, 1});
  const auto add_state = [&](srhd_awa::platform::ui::GraphButtonSlot slot,
                             Kind kind, const char* resource) {
    auto image = std::make_unique<UiImageLeaf>(&package);
    Check(image->Load(kind, resource, "", &error), error.c_str());
    Check(button->AddStateImage(slot, std::move(image), &error), error.c_str());
  };
  using srhd_awa::platform::ui::GraphButtonSlot;
  add_state(GraphButtonSlot::Normal, Kind::Simple, "S.BMP");
  add_state(GraphButtonSlot::NormalA, Kind::Simple, "I.BMP");
  add_state(GraphButtonSlot::Down, Kind::Trans, "T.BMP");
  add_state(GraphButtonSlot::DisableA, Kind::Alpha, "A.PSD");
  add_state(GraphButtonSlot::Hit, Kind::Alpha, "A.PSD");
  std::vector<std::uint16_t> button_pixels(15, 0);
  Framebuffer button_frame{button_pixels.data(), 5, 3, 5};
  Check(button_tree.Render(button_frame, &error) && button_pixels[6] == 0x001f,
      "GraphButton normal child image");
  button->SetHovered(true); button_pixels.assign(15, 0);
  Check(button_tree.Render(button_frame, &error) && button_pixels[6] == 0xf800,
      "GraphButton hovered child image");
  button->SetDown(true); button_pixels.assign(15, 0);
  Check(button->VisualSlot() == GraphButtonSlot::Down &&
      button_tree.Render(button_frame, &error) && button_pixels[7] == 0x07e0,
      "GraphButton missing DownA fallback and Trans image");
  button->SetDown(false); button->SetHovered(false);
  button->SetHitKind(srhd_awa::platform::ui::GraphButtonHitKind::Graph);
  Check(button->HitTest({1, 1}) && !button->HitTest({0, 1}),
      "GraphButton Graph hit checks inactive Alpha state");
  button->SetHitKind(srhd_awa::platform::ui::GraphButtonHitKind::ImageHit);
  Check(button->HitTest({1, 1}) && !button->StateImage(GraphButtonSlot::Hit)->Active(),
      "GraphButton ImageHit is hit-only");
  button->SetPosition({2, 1});
  Check(button->HitTest({2, 1}) && !button->HitTest({1, 1}),
      "GraphButton hit image follows button move");
  std::remove(path); std::puts("UI TREE RENDER TEST PASS");
}

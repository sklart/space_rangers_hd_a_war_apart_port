#include "aft_font.hpp"
#include "m23_aft_fixture.hpp"
#include "package.hpp"
#include "ui_controls_fingerprint.hpp"
#include "ui_controls_checkpoint.hpp"
#include "ui_graph_button.hpp"
#include "ui_label.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_window.hpp"
#include "ui_zone.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using srhd_awa::package::Package;
using srhd_awa::platform::aft_font::AftFont;
using srhd_awa::platform::image_object::Kind;
using srhd_awa::platform::scene_compositor::Framebuffer;
using namespace srhd_awa::platform::ui;
using srhd_awa::platform::ui_fingerprint::Value;

void CheckLine(bool ok, const char* what, int line) {
  if (!ok) { std::fprintf(stderr, "FAIL line %d: %s\n", line, what); std::exit(1); }
}
#define Check(ok, what) CheckLine((ok), (what), __LINE__)
void U32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i{}; i < 4; ++i) (*bytes)[at + i] = value >> (i * 8);
}
void BE16(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint16_t value) {
  (*bytes)[at] = value >> 8; (*bytes)[at + 1] = value;
}
void BE32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i{}; i < 4; ++i) (*bytes)[at + i] = value >> (24 - i * 8);
}
std::vector<std::uint8_t> Bmp(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  std::vector<std::uint8_t> bytes(58);
  bytes[0] = 'B'; bytes[1] = 'M'; U32(&bytes, 2, bytes.size());
  U32(&bytes, 10, 54); U32(&bytes, 14, 40); U32(&bytes, 18, 1); U32(&bytes, 22, 1);
  bytes[26] = 1; bytes[28] = 24; U32(&bytes, 34, 4);
  bytes[54] = blue; bytes[55] = green; bytes[56] = red;
  return bytes;
}
std::vector<std::uint8_t> Gi(std::uint16_t color) {
  std::vector<std::uint8_t> bytes(104); std::memcpy(bytes.data(), "gi\0", 3);
  U32(&bytes, 4, 1); U32(&bytes, 16, 2); U32(&bytes, 20, 2);
  U32(&bytes, 24, 0xf800); U32(&bytes, 28, 0x07e0); U32(&bytes, 32, 0x001f);
  U32(&bytes, 44, 1); U32(&bytes, 64, 96); U32(&bytes, 68, 8);
  U32(&bytes, 80, 2); U32(&bytes, 84, 2);
  for (std::size_t at = 96; at < 104; at += 2) {
    bytes[at] = color; bytes[at + 1] = color >> 8;
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
      {"B.BMP", Bmp(0, 0, 255)}, {"R.BMP", Bmp(255, 0, 0)},
      {"G.BMP", Bmp(0, 255, 0)}, {"Y.BMP", Bmp(255, 255, 0)},
      {"GI.GAI", Gai()}};
  constexpr std::size_t root = 4, directory = 12 + 5 * 158;
  std::size_t cursor = root + directory; std::vector<std::uint8_t> bytes(cursor);
  U32(&bytes, 0, root); U32(&bytes, root, directory);
  U32(&bytes, root + 4, items.size()); U32(&bytes, root + 8, 158);
  for (std::size_t i{}; i < items.size(); ++i) {
    const auto target = cursor; bytes.resize(cursor + 4 + items[i].second.size());
    U32(&bytes, cursor, items[i].second.size());
    std::memcpy(bytes.data() + cursor + 4, items[i].second.data(), items[i].second.size());
    Entry(&bytes, root + 12 + i * 158, items[i].first, target, items[i].second.size());
    cursor = bytes.size();
  }
  FILE* file = std::fopen(path, "wb"); if (!file) return false;
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  std::fclose(file); return ok;
}
std::unique_ptr<UiImageLeaf> Image(Package* package, const char* name, const char* resource) {
  auto image = std::make_unique<UiImageLeaf>(package); image->SetName(name);
  std::string error; Check(image->Load(Kind::Simple, resource, "", &error), error.c_str());
  return image;
}
void CheckValue(Value actual, std::uint32_t crc, std::uint64_t fnv, const char* name) {
  if (actual.crc32 != crc || actual.fnv64 != fnv) {
    std::fprintf(stderr, "FAIL %s actual=%08x/%016llx bytes=%zu expected=%08x/%016llx\n",
                 name, actual.crc32, static_cast<unsigned long long>(actual.fnv64),
                 actual.bytes, crc, static_cast<unsigned long long>(fnv));
    std::exit(1);
  }
}
}  // namespace

int main() {
  constexpr const char* path = "build/test_m24_controls.pkg";
  Check(WritePackage(path), "write M24 package fixture");
  Package package; std::string error;
  Check(package.Open(path, &error), error.c_str());
  const auto source = m23_test::AftFixture();
  auto font = std::make_shared<AftFont>();
  Check(font->Load(source.data(), source.size(), &error), error.c_str());
  UiTree tree; tree.SetRootSize({12, 10});
  auto* window = tree.Root()->AddWindow(); window->SetName("window");
  window->SetSize({10, 8}); window->SetPosition({1, 1});
  window->SetMinimumSize({10, 8}); window->SetWorkSubRect({1, 1, 9, 7});
  constexpr struct { WindowSlot slot; const char* name; } border[] = {
      {WindowSlot::Left, "left"}, {WindowSlot::Right, "right"},
      {WindowSlot::Top, "top"}, {WindowSlot::Bottom, "bottom"},
      {WindowSlot::TopLeft, "top-left"}, {WindowSlot::TopRight, "top-right"},
      {WindowSlot::BottomLeft, "bottom-left"},
      {WindowSlot::BottomRight, "bottom-right"},
      {WindowSlot::Texture, "texture"}};
  for (const auto& [slot, name] : border)
    Check(window->AddBorderImage(slot, Image(&package,
          (std::string("window-") + name).c_str(), "B.BMP"), &error), error.c_str());
  Check(window->FinalizeLayout(&error), error.c_str());
  auto* a = window->AddGraphButton(); a->SetName("button-a");
  a->SetPosition({2, 1}); a->SetSize({5, 6}); a->SetDepth(20);
  Check(a->AddStateImage(GraphButtonSlot::Normal, Image(&package, "a-normal", "R.BMP"), &error), error.c_str());
  Check(a->AddStateImage(GraphButtonSlot::NormalA, Image(&package, "a-normal-a", "G.BMP"), &error), error.c_str());
  Check(a->AddStateImage(GraphButtonSlot::Down, Image(&package, "a-down", "Y.BMP"), &error), error.c_str());
  auto caption = std::make_unique<UiLabelLeaf>(); caption->SetName("caption");
  caption->SetFont("fixture", font); caption->SetTextLines({u"A"});
  caption->SetAlignX(LabelAlignX::Left); caption->SetAlignY(LabelAlignY::Top);
  Check(a->AddCaption(std::move(caption), &error), error.c_str());
  a->SetCaptionColors({0xffff, 0x07e0, 0xffe0, 0xffff, 0xffff, 0xffff});
  a->SetCaptionOffsets({0, 0}, {1, 0});
  Check(a->Caption()->Prepare(&error), error.c_str());
  auto* b = window->AddGraphButton(); b->SetName("button-b");
  b->SetPosition({7, 2}); b->SetSize({2, 2}); b->SetDepth(10);
  Check(b->AddStateImage(GraphButtonSlot::Normal, Image(&package, "b-normal", "R.BMP"), &error), error.c_str());
  auto* label = window->AddLabel(); label->SetName("label");
  label->SetPosition({0, 0}); label->SetSize({5, 5}); label->SetDepth(5);
  label->SetFont("fixture", font); label->SetTextLines({u"A"});
  label->SetAlignX(LabelAlignX::Left); label->SetAlignY(LabelAlignY::Top);
  Check(label->Prepare(&error), error.c_str());
  auto* gi = window->AddGIObject(); gi->SetName("gi");
  gi->SetPosition({7, 5}); gi->SetDepth(4);
  gi->Image().SetPackage(&package);
  Check(gi->LoadResource("GI.GAI", &error), error.c_str());
  auto* zone = window->AddZone(); zone->SetName("zone");
  zone->SetPosition({3, 4}); zone->SetSize({2, 2});
  zone->SetZoneKind(ZoneKind::Circle);
  const std::vector<Point> hit_points{{4, 5}, {5, 6}, {6, 6}, {4, 6}, {3, 5}, {7, 7}};
  Value result{};
  Check(srhd_awa::platform::ui_controls_fingerprint::ComputeWindowLayout(*window, &result),
        "Window layout fingerprint");
  CheckValue(result, 0x80b94d87u, 0x77ccd5f317a573d6ull, "Python Window layout");
  zone->SetZoneKind(ZoneKind::Rect);
  Check(srhd_awa::platform::ui_controls_fingerprint::ComputeZoneHits(*zone, hit_points, &result),
        "Zone Rect fingerprint");
  CheckValue(result, 0x9dd285e1u, 0x20ffbea85aa4e660ull, "Python Zone Rect");
  zone->SetZoneKind(ZoneKind::Circle);
  Check(srhd_awa::platform::ui_controls_fingerprint::ComputeZoneHits(*zone, hit_points, &result),
        "Zone Circle fingerprint");
  CheckValue(result, 0x8ae2e69eu, 0xd34c2faf3f348e0full, "Python Zone Circle");
  std::vector<std::uint16_t> pixels(120, 0);
  Framebuffer frame{pixels.data(), 12, 10, 12};
  constexpr std::uint32_t tree_crc[] = {0x6f65eea5u, 0x7110370fu, 0x60f9638eu};
  constexpr std::uint64_t tree_fnv[] = {0x11b73bf4b18ab6fbull, 0x96c53d4655468267ull,
                                         0x2e2801971a2b76c1ull};
  constexpr std::uint32_t graph_crc[] = {0x6bff73cbu, 0x4eb9fddbu, 0xccacf19au};
  constexpr std::uint64_t graph_fnv[] = {0xe1747b752476d6d6ull, 0x26b55b559b3eabb9ull,
                                          0x3803a382dc99778dull};
  constexpr std::uint32_t frame_crc[] = {0x015d1589u, 0x680b03b6u, 0xede9bc02u};
  constexpr std::uint64_t frame_fnv[] = {0x5523d509a3a9384dull, 0xe0b970ea3c332ce8ull,
                                          0x1a306cb1aaa33080ull};
  for (int state = 0; state < 3; ++state) {
    if (state == 1) { a->SetHovered(true); Check(tree.Update(10, &error), error.c_str()); }
    if (state == 2) { a->SetHovered(false); a->SetDown(true); }
    pixels.assign(120, 0);
    Check(tree.Render(frame, &error), error.c_str());
    Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*tree.Root(), &result, &error), error.c_str());
    CheckValue(result, tree_crc[state], tree_fnv[state], "Python M24 tree");
    Check(srhd_awa::platform::ui_controls_fingerprint::ComputeGraphState(*a, &result),
          "GraphButton state fingerprint");
    CheckValue(result, graph_crc[state], graph_fnv[state], "Python GraphButton state");
    Check(srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(frame, &result, &error), error.c_str());
    CheckValue(result, frame_crc[state], frame_fnv[state], "Python M24 framebuffer");
  }
  srhd_awa::platform::ui_controls_checkpoint::Checkpoint runtime_checkpoint;
  srhd_awa::platform::ui_controls_checkpoint::Evidence evidence{};
  Check(runtime_checkpoint.Initialize(&error) &&
        runtime_checkpoint.VerifyFixed(&evidence, &error), error.c_str());
  CheckValue(evidence.frame, frame_crc[0], frame_fnv[0],
             "in-memory runtime M24 checkpoint matches independent oracle");
  Check(runtime_checkpoint.StartDynamic(&error) && runtime_checkpoint.Update(1000, &error),
        error.c_str());
  std::remove(path);
  std::puts("M24 CONTROLS SYNTHETIC ORACLE PASS");
}

#include "image_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_tree_renderer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
using srhd_awa::package::Package;
using srhd_awa::platform::image_object::Kind;
using srhd_awa::platform::scene_compositor::Framebuffer;
using srhd_awa::platform::ui::Point;
using srhd_awa::platform::ui::Size;
using srhd_awa::platform::ui::UiImageLeaf;
using srhd_awa::platform::ui::UiTree;
using srhd_awa::platform::ui::UiTreeRenderer;

void U32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index) (*bytes)[at + index] = static_cast<std::uint8_t>(value >> (index * 8));
}
std::vector<std::uint8_t> Bmp(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  std::vector<std::uint8_t> bytes(58); bytes[0] = 'B'; bytes[1] = 'M'; U32(&bytes, 2, bytes.size()); U32(&bytes, 10, 54);
  U32(&bytes, 14, 40); U32(&bytes, 18, 1); U32(&bytes, 22, 1); bytes[26] = 1; bytes[28] = 24; U32(&bytes, 34, 4);
  bytes[54] = blue; bytes[55] = green; bytes[56] = red; return bytes;
}
void Entry(std::vector<std::uint8_t>* bytes, std::size_t at, const char* name, std::uint32_t target, std::uint32_t size) {
  std::memset(bytes->data() + at, 0, 158); U32(bytes, at + 4, size); std::strncpy(reinterpret_cast<char*>(bytes->data() + at + 8), name, 62);
  std::strncpy(reinterpret_cast<char*>(bytes->data() + at + 71), name, 62); U32(bytes, at + 150, target);
}
bool WritePackage(const char* path) {
  const auto blue = Bmp(0, 0, 255); const auto red = Bmp(255, 0, 0); constexpr std::size_t root = 4, directory = 12 + 2 * 158;
  std::vector<std::uint8_t> bytes(root + directory); U32(&bytes, 0, root); U32(&bytes, root, directory); U32(&bytes, root + 4, 2); U32(&bytes, root + 8, 158);
  std::size_t cursor = bytes.size(); const auto add = [&](const char* name, const std::vector<std::uint8_t>& payload, std::size_t index) {
    const auto target = cursor; bytes.resize(cursor + 4 + payload.size()); U32(&bytes, cursor, payload.size()); std::memcpy(bytes.data() + cursor + 4, payload.data(), payload.size());
    Entry(&bytes, root + 12 + index * 158, name, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(payload.size())); cursor = bytes.size();
  };
  add("BLUE.BMP", blue, 0); add("RED.BMP", red, 1); FILE* file = std::fopen(path, "wb"); if (!file) return false;
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(); std::fclose(file); return ok;
}
void Check(bool value, const char* what) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); } }
UiImageLeaf* AddImage(srhd_awa::platform::ui::UiObject* parent, Package* package, const char* resource, Point position, Size size, double depth, bool active = true) {
  auto leaf = std::make_unique<UiImageLeaf>(package); auto* result = leaf.get(); result->SetPosition(position); result->SetSize(size); result->SetDepth(depth); result->SetActive(active);
  result->Image().SetModes(srhd_awa::platform::image_layout::XMode::LeftFill, srhd_awa::platform::image_layout::YMode::TopFill);
  std::string error; Check(result->Load(Kind::Simple, resource, "", &error), error.c_str()); Check(parent->Attach(std::move(leaf), &error), error.c_str()); return result;
}
}  // namespace

int main() {
  constexpr const char* path = "build/test_ui_tree.pkg"; Check(WritePackage(path), "write package");
  Package package; std::string error; Check(package.Open(path, &error), error.c_str()); UiTree tree; auto* root = tree.Root(); root->SetSize({4, 2});
  AddImage(root, &package, "BLUE.BMP", {0, 0}, {4, 2}, 100);
  auto* panel = root->AddPanel(); panel->SetPosition({1, 0}); panel->SetSize({2, 2}); panel->SetDepth(0);
  AddImage(panel, &package, "RED.BMP", {0, 0}, {4, 2}, 0);
  AddImage(root, &package, "RED.BMP", {0, 0}, {4, 2}, -100, false);
  std::vector<std::uint16_t> pixels(8, 0); Framebuffer framebuffer{pixels.data(), 4, 2, 4};
  Check(UiTreeRenderer::Render(*root, framebuffer, &error), error.c_str());
  Check(pixels[0] == 0x001f && pixels[1] == 0xf800 && pixels[2] == 0xf800 && pixels[3] == 0x001f, "panel clip and depth traversal");
  Check(pixels[4] == 0x001f && pixels[5] == 0xf800 && pixels[6] == 0xf800 && pixels[7] == 0x001f, "inactive leaf skipped");
  std::remove(path); std::puts("UI TREE RENDER TEST PASS");
}

#include "image_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_graph_button.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_window.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform;
void Check(bool condition, const std::string& reason) {
  if (!condition) { std::fprintf(stderr, "M25 CONTROLS RELEASE FAIL: %s\n", reason.c_str()); std::exit(1); }
}
std::unique_ptr<ui::UiImageLeaf> Image(srhd_awa::package::Package* package,
                                       const char* resource) {
  auto image = std::make_unique<ui::UiImageLeaf>(package);
  std::string error;
  Check(image->Load(image_object::Kind::GI, resource, "", &error), error);
  return image;
}
ui_fingerprint::Value Frame(ui::UiTree& tree, std::int32_t width, std::int32_t height) {
  std::vector<std::uint16_t> pixels(static_cast<std::size_t>(width) * height, 0);
  scene_compositor::Framebuffer target{pixels.data(), width, height, width};
  std::string error;
  Check(tree.Render(target, &error), error);
  ui_fingerprint::Value result{};
  Check(ui_fingerprint::ComputeFramebuffer(target, &result, &error), error);
  return result;
}
void Hash(ui_fingerprint::Value value, std::uint32_t crc, std::uint64_t fnv,
          const char* label) {
  std::printf("M25 %s %08x/%016llx\n", label, value.crc32,
              static_cast<unsigned long long>(value.fnv64));
  Check(value.crc32 == crc && value.fnv64 == fnv, label);
}
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  srhd_awa::package::Package package;
  std::string error;
  Check(package.Open(std::string(argv[1]) + "/DATA/forms.pkg", &error), error);
  ui::UiTree button_tree;
  button_tree.SetRootSize({53, 42});
  auto* button = button_tree.Root()->AddGraphButton();
  button->SetName("F1"); button->SetSize({53, 42});
  button->SetButtonKind(ui::GraphButtonKind::Disable);
  button->SetHitKind(ui::GraphButtonHitKind::Graph);
  constexpr struct { ui::GraphButtonSlot slot; const char* path; } states[] = {
      {ui::GraphButtonSlot::Normal, "DATA/FormAB2/2W1GN.gi"},
      {ui::GraphButtonSlot::NormalA, "DATA/FormAB2/2W1GA.gi"},
      {ui::GraphButtonSlot::Down, "DATA/FormAB2/2W1GD.gi"},
      {ui::GraphButtonSlot::Disable, "DATA/FormAB2/2W1H.gi"}};
  for (const auto& state : states)
    Check(button->AddStateImage(state.slot, Image(&package, state.path), &error), error);
  Hash(Frame(button_tree, 53, 42), 0x1c585c8fu, 0x7f23dbb7c1d8df15ull, "GraphButton normal");
  button->SetHovered(true);
  Hash(Frame(button_tree, 53, 42), 0x1a6eae32u, 0x46793341f42dd912ull, "GraphButton hover");
  button->SetDown(true);
  Hash(Frame(button_tree, 53, 42), 0x663cb736u, 0xc45ecedf7f592470ull, "GraphButton down");
  Check(button->HitTest({26, 20}, &error), "real GI Graph hit");

  ui::UiTree window_tree;
  auto* window = window_tree.Root()->AddWindow();
  window->SetName("InfoPanel"); window->SetSize({280, 174});
  window->SetMinimumSize({250, 0}); window->SetWorkSubRect({16, 65, 12, 15});
  constexpr struct { ui::WindowSlot slot; const char* path; } borders[] = {
      {ui::WindowSlot::Left, "DATA/FormNote/2SimpleLeft.gi"},
      {ui::WindowSlot::Right, "DATA/FormNote/2SimpleRight.gi"},
      {ui::WindowSlot::Top, "DATA/FormNote/2SimpleTop.gi"},
      {ui::WindowSlot::Bottom, "DATA/FormNote/2SimpleBottom.gi"},
      {ui::WindowSlot::TopLeft, "DATA/FormNote/2SimpleTopLeft.gi"},
      {ui::WindowSlot::TopRight, "DATA/FormNote/2SimpleTopRight.gi"},
      {ui::WindowSlot::BottomLeft, "DATA/FormNote/2SimpleBottomLeft.gi"},
      {ui::WindowSlot::BottomRight, "DATA/FormNote/2SimpleBottomRight.gi"},
      {ui::WindowSlot::Texture, "DATA/FormNote/2SimpleTexture.gi"}};
  for (const auto& border : borders)
    Check(window->AddBorderImage(border.slot, Image(&package, border.path), &error), error);
  Check(window->FinalizeLayout(&error), error);
  Check(window->ClientSize() == ui::Size{282, 175}, "Window aligned size");
  window_tree.SetRootSize(window->ClientSize());
  Hash(Frame(window_tree, 282, 175), 0x87e5a68fu, 0x8020187c43bbcc26ull, "Window border");
  std::puts("M25 REAL GRAPHBUTTON WINDOW PASS");
}

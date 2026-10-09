#include "ec_file_adapter.hpp"
#include "font_repository.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_cache_resolver.hpp"
#include "ui_config.hpp"
#include "ui_edit.hpp"
#include "ui_graph_buffer.hpp"
#include "ui_panel_scroll_bar.hpp"
#include "ui_scroll_bar.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <zlib.h>

namespace {
using namespace srhd_awa::platform;
class ReleaseFontResolver final : public font_repository::IFontResolver {
 public:
  bool Open(const std::string& root, std::string* error) {
    return package_.Open(root + "/DATA/forms.pkg", error);
  }
  bool LoadFont(const std::string& key, std::vector<std::uint8_t>* bytes,
                std::string* source, std::string* error) override {
    const std::pair<const char*, const char*> fonts[] = {
        {"Font.2Normal", "DATA/FONT/Verdana_09_2.aft"},
        {"Font.2Ranger", "DATA/FONT/ranger_6.aft"},
        {"Font.2Small", "DATA/FONT/Verdana_08_1.aft"},
        {"Font.2SmallBold", "DATA/FONT/Verdana_08_2_bold.aft"}};
    for (const auto& [name, path] : fonts) if (key == name) {
      const auto* entry = package_.Resolve(path);
      if (!entry || !package_.ReadPayload(*entry, bytes, error)) return false;
      if (source) *source = path;
      return true;
    }
    if (error) *error = "unmapped release font fixture";
    return false;
  }
 private:
  srhd_awa::package::Package package_;
};
void Check(bool value, const std::string& reason) {
  if (!value) { std::fprintf(stderr, "M26 REAL SCROLL FAIL: %s\n", reason.c_str()); std::exit(1); }
}
std::uint64_t Fnv(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t value = UINT64_C(0xcbf29ce484222325);
  for (auto byte : bytes) value = (value ^ byte) * UINT64_C(0x100000001b3);
  return value;
}
std::size_t EstimatedImageBytes(const ui::UiObject& object) {
  std::size_t total{};
  if (const auto* leaf = dynamic_cast<const ui::UiImageLeaf*>(&object))
    total += static_cast<std::size_t>(std::max(0, leaf->Image().natural_width())) *
             static_cast<std::size_t>(std::max(0, leaf->Image().natural_height())) * 4;
  for (const auto& child : object.Children()) total += EstimatedImageBytes(*child);
  return total;
}
EC_BlockPar::TBlockParEC* Child(EC_BlockPar::TBlockParEC* parent,
                               const char* name, int occurrence) {
  if (!parent) return nullptr;
  for (int index = 0, seen = 0; index < parent->GetBlockCount(); ++index) {
    if (parent->GetBlockNameByIndex(index) == pas::WideString(name) &&
        seen++ == occurrence) return parent->GetBlockByIndex(index);
  }
  return nullptr;
}
EC_BlockPar::TBlockParEC* Find(EC_BlockPar::TBlockParEC* root,
    std::initializer_list<std::pair<const char*, int>> path) {
  for (auto [name, index] : path) root = Child(root, name, index);
  return root;
}
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  try {
  std::string error;
  std::fprintf(stderr, "M26 release: init\n");
  ec_file::SetGameRoot(argv[1]);
  Check(aPacket::InitializePackageCollection(), "package collection initialization");
  GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
  GR_Main::SelectedLanguage = u"russian"_w;
  GR_Main::LoadLanguageAndPackages();
  std::fprintf(stderr, "M26 release: Main.dat\n");
  auto* config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  config->LoadFromEncryptedDatFile(u"CFG/Main.dat");
  auto* cache = pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
  cache->LoadFromEncryptedDatFile(u"CFG/CacheData.dat");
  std::fprintf(stderr, "M26 release: select controls\n");
  ui_cache_resolver::CacheUiResourceResolver resolver(cache);
  ReleaseFontResolver font_resolver;
  Check(font_resolver.Open(argv[1], &error), error);
  font_repository::Repository fonts(&font_resolver);
  ui_config::Context context{};
  context.resources = &resolver;
  context.fonts = &fonts;
  auto* language = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  language->LoadFromEncryptedDatFile(u"CFG/Rus/Lang.dat");
  context.language = language;
  context.resolve_label_font_alias = [](const std::string& key) { return key; };
  context.styles = Child(config, "ML", 0);
  auto* depth = config->FindBlock(u"ZPos");
  context.resolve_depth = [depth](const std::string& name, double* value) {
    return ui_config::ResolveRuntimeDepth(depth, name, value);
  };
  auto* scroll_source = Find(config, {{"ML", 0}, {"Film", 0}, {"Panel", 0},
                                      {"Panel", 0}, {"Panel", 0}, {"ScrollBar", 0}});
  auto* panel_source = Find(config, {{"ML", 0}, {"Achievements", 0}, {"Panel", 0},
                                     {"Panel", 0}, {"PanelScrollBar", 0}});
  Check(scroll_source && panel_source, "Main.dat source paths changed");
  ui::UiTree tree;
  tree.SetRootSize({1280, 720});
  std::fprintf(stderr, "M26 release: factory\n");
  Check(ui_config::LoadSelectedControl(tree.Root(), "ScrollBar", scroll_source,
                                       context, true, &error) &&
        ui_config::LoadSelectedControl(tree.Root(), "PanelScrollBar", panel_source,
                                       context, false, &error), error);
  auto* scroll = dynamic_cast<ui::UiScrollBar*>(tree.Root()->FindByNameRecursive("PF_SBTurn"));
  auto* panel = dynamic_cast<ui::UiPanelScrollBar*>(tree.Root()->FindByNameRecursive("PanelSlot"));
  Check(scroll && panel && scroll->Orientation() == 1 &&
        scroll->ClientSize() == ui::Size{251, 19} && scroll->Position() == 1,
        "real ScrollBar configuration");
  Check(scroll->Image(ui::ScrollBarPart::Up, ui::ScrollBarState::Normal) &&
        scroll->Image(ui::ScrollBarPart::ThumbCenter, ui::ScrollBarState::Down) &&
        scroll->Image(ui::ScrollBarPart::Down, ui::ScrollBarState::Active),
        "real ScrollBar GI states loaded");
  Check(scroll->TrackLength() == 209 && scroll->ThumbLength() == 56 &&
        scroll->BeforeLength() == 0 && scroll->AfterLength() == 153 &&
        !scroll->Narrow(), "independent release ScrollBar layout oracle");
  std::printf("M26 REAL SCROLL layout track=%d thumb=%d before=%d after=%d narrow=%d\n",
      scroll->TrackLength(), scroll->ThumbLength(), scroll->BeforeLength(),
      scroll->AfterLength(), scroll->Narrow());
  Check(panel->External() && !panel->UnlimitedWorld() &&
        panel->VerticalBar()->Active() &&
        panel->VerticalBar()->Parent() == tree.Root() &&
        panel->VerticalBar()->LocalPosition() == ui::Point{903, 116} &&
        panel->VerticalBar()->ClientSize() == ui::Size{20, 513},
        "real external PanelScrollBar geometry");
  Check(panel->UpdateScrollRanges(&error), error);
  Check(panel->VerticalBar()->Minimum() == 0 &&
        panel->VerticalBar()->Maximum() == 539 &&
        panel->VerticalBar()->PageSize() == 540 &&
        panel->VerticalBar()->Position() == 0,
        "independent release PanelScrollBar range oracle");
  std::printf("M26 REAL PANEL range=%d,%d page=%d position=%d\n",
      panel->VerticalBar()->Minimum(), panel->VerticalBar()->Maximum(),
      panel->VerticalBar()->PageSize(), panel->VerticalBar()->Position());
  const auto bar_position = panel->VerticalBar()->LocalPosition();
  const auto bar_size = panel->VerticalBar()->ClientSize();
  const std::array<std::int32_t, 10> panel_words{
      bar_position.x, bar_position.y, bar_position.x + bar_size.width,
      bar_position.y + bar_size.height, bar_size.width, bar_size.height,
      panel->VerticalBar()->Minimum(), panel->VerticalBar()->Maximum(),
      panel->VerticalBar()->PageSize(), panel->VerticalBar()->Position()};
  const auto* panel_bytes = reinterpret_cast<const std::uint8_t*>(panel_words.data());
  const std::vector<std::uint8_t> panel_layout(panel_bytes,
                                                panel_bytes + sizeof(panel_words));
  const auto panel_crc = crc32(0, panel_layout.data(), panel_layout.size());
  const auto panel_fnv = Fnv(panel_layout);
  Check(panel_crc == 0x3334a660u &&
        panel_fnv == UINT64_C(0x82cd5b1de32b0ba2),
        "independent Python PanelScrollBar layout fingerprint");
  std::printf("M26 REAL PANEL layout=%08x/%016llx\n",
              static_cast<unsigned>(panel_crc),
              static_cast<unsigned long long>(panel_fnv));
  auto* showcase_source = Find(config, {{"ML", 0}, {"Info", 0}, {"Panel", 0},
      {"Panel", 4}, {"Panel", 11}});
  Check(showcase_source && showcase_source->GetBlockCount() == 49,
        "M26 50-node showcase source path changed");
  ui::UiTree showcase;
  showcase.SetRootSize({1280, 720});
  auto showcase_panel = std::make_unique<ui::UiPanel>();
  Check(ui_config::ApplyBaseProperties(showcase_panel.get(), showcase_source,
                                        context, &error), error);
  Check(!showcase_panel->Active(), "selected release tab starts hidden");
  showcase_panel->SetActive(true);  // Show the real tab without altering its contents.
  showcase_panel->SetPosition({298, 120});
  Check(showcase_panel->ClientSize() == ui::Size{410, 435},
        "showcase release dimensions");
  Check(ui_config::LoadChildren(showcase_panel.get(), showcase_source,
                                context, ui_config::LoadMode::Strict, nullptr,
                                &error), error);
  Check(showcase_panel->ChildCount() == 49, "showcase release child count");
  auto* showcase_node = showcase_panel.get();
  Check(showcase.Root()->Attach(std::move(showcase_panel), &error), error);
  std::vector<std::uint16_t> showcase_pixels(1280 * 720);
  scene_compositor::Framebuffer showcase_target{showcase_pixels.data(), 1280, 720, 1280};
  Check(showcase.Render(showcase_target, &error), error);
  ui_fingerprint::Value showcase_frame{};
  Check(ui_fingerprint::ComputeFramebuffer(showcase_target, &showcase_frame, &error), error);
  {
    std::ofstream frame_file("build/m26-showcase-host.rgb565", std::ios::binary);
    Check(static_cast<bool>(frame_file), "showcase diagnostic output path");
    frame_file.write(reinterpret_cast<const char*>(showcase_pixels.data()),
                     static_cast<std::streamsize>(showcase_pixels.size() * sizeof(std::uint16_t)));
  }
  ui_fingerprint::Value showcase_tree{};
  Check(ui_fingerprint::ComputeTree(*showcase_node, &showcase_tree, &error), error);
  std::printf("M26 REAL SHOWCASE tree=%08x/%016llx bytes=%zu\n",
      showcase_tree.crc32, static_cast<unsigned long long>(showcase_tree.fnv64),
      showcase_tree.bytes);
  Check(showcase_tree.crc32 == 0x67b1fcbfu &&
        showcase_tree.fnv64 == UINT64_C(0x2940a0ef334dd86e),
        "M26 fixed 50-node tree fingerprint");
  std::printf("M26 REAL SHOWCASE nodes=50 position=298,120 size=410x435 frame=%08x/%016llx\n",
      showcase_frame.crc32, static_cast<unsigned long long>(showcase_frame.fnv64));
  Check(showcase_frame.crc32 == 0xcb1a12b3u &&
        showcase_frame.fnv64 == UINT64_C(0xfbca196e86b2f452),
        "independent Python real release showcase frame oracle");
  auto* edit = dynamic_cast<ui::UiEdit*>(showcase.Root()->FindByNameRecursive("M11Size"));
  Check(edit && edit->Text().empty() && edit->ClientSize() == ui::Size{99, 17},
        "real Edit config defaults");
  edit->SetText(u"123");
  std::vector<std::uint16_t> edit_pixels(1280 * 720);
  scene_compositor::Framebuffer edit_target{edit_pixels.data(), 1280, 720, 1280};
  ui_fingerprint::Value unfocused_frame{};
  Check(edit->RenderLeaf(edit_target, {0, 0, 1280, 720}, &error) &&
        ui_fingerprint::ComputeFramebuffer(edit_target, &unfocused_frame, &error), error);
  Check(unfocused_frame.crc32 == 0x52c44a22u &&
        unfocused_frame.fnv64 == UINT64_C(0x490042511484a325),
        "independent Python unfocused Edit frame oracle");
  edit->SetFocused(true);
  edit->SetCaretBlink(false);
  std::fill(edit_pixels.begin(), edit_pixels.end(), 0);
  ui_fingerprint::Value caret_off_frame{};
  Check(edit->RenderLeaf(edit_target, {0, 0, 1280, 720}, &error) &&
        ui_fingerprint::ComputeFramebuffer(edit_target, &caret_off_frame, &error), error);
  Check(caret_off_frame.crc32 == unfocused_frame.crc32 &&
        caret_off_frame.fnv64 == unfocused_frame.fnv64,
        "independent Python focused caret-off Edit frame oracle");
  edit->SetCaretBlink(true);
  std::fill(edit_pixels.begin(), edit_pixels.end(), 0);
  Check(edit->RenderLeaf(edit_target, {0, 0, 1280, 720}, &error), error);
  ui_fingerprint::Value edit_frame{};
  Check(ui_fingerprint::ComputeFramebuffer(edit_target, &edit_frame, &error), error);
  Check(edit_frame.crc32 == 0x0c547e1bu &&
        edit_frame.fnv64 == UINT64_C(0x18432c76a6d566c5),
        "independent Python release Edit caret oracle");
  std::printf("M26 REAL EDIT frame=%08x/%016llx absolute=%d,%d\n",
      edit_frame.crc32, static_cast<unsigned long long>(edit_frame.fnv64),
      edit->AbsolutePosition().x, edit->AbsolutePosition().y);
  srhd_awa::package::Package graph_package;
  Check(graph_package.Open(std::string(argv[1]) + "/DATA/forms.pkg", &error), error);
  const auto* graph_entry = graph_package.Resolve("DATA/FormLoad2/2BarCenter.gi");
  std::vector<std::uint8_t> graph_source;
  Check(graph_entry && graph_package.ReadPayload(*graph_entry, &graph_source, &error), error);
  ui::UiTree graph_tree;
  graph_tree.SetRootSize({1280, 720});
  auto* graph = graph_tree.Root()->AddGraphBuffer();
  graph->SetPosition({1040, 95});
  graph->SetSize({100, 50});
  Check(graph->LoadGiBytes(graph_source.data(), graph_source.size(), &error) &&
        graph->Buffer().width() == 39 && graph->Buffer().height() == 50 &&
        graph->Buffer().bytes() == 7800, "real GI GraphBuf aspect-fit oracle");
  const auto& graph_bytes = graph->Buffer().pixels();
  Check(graph_bytes.size() == 7800 &&
        crc32(0, graph_bytes.data(), graph_bytes.size()) == 0x07bfadc2u &&
        Fnv(graph_bytes) == UINT64_C(0x1dc1b91c9c87ca5a),
        "real GI GraphBuf scaled host/ARM-compatible fingerprint");
  std::printf("M26 REAL GRAPHBUF scaled=%08x/%016llx\n",
      static_cast<unsigned>(crc32(0, graph_bytes.data(), graph_bytes.size())),
      static_cast<unsigned long long>(Fnv(graph_bytes)));
  const auto graph_center = graph->GetVisualCenter();
  std::printf("M26 REAL GRAPHBUF hit=%d center=%d,%d\n",
      graph->HitTestPixel({1090, 120}), graph_center.x, graph_center.y);
  std::vector<std::uint16_t> graph_pixels(1280 * 720);
  scene_compositor::Framebuffer graph_target{graph_pixels.data(), 1280, 720, 1280};
  Check(graph_tree.Render(graph_target, &error), error);
  ui_fingerprint::Value graph_frame{};
  Check(ui_fingerprint::ComputeFramebuffer(graph_target, &graph_frame, &error), error);
  Check(graph_frame.crc32 == 0x1ebb3fc8u &&
        graph_frame.fnv64 == UINT64_C(0xeb791740957eeeb8),
        "real GI GraphBuf host frame fingerprint");
  std::size_t font_source_bytes{};
  for (const auto* path : {"DATA/FONT/Verdana_09_2.aft", "DATA/FONT/ranger_6.aft",
                           "DATA/FONT/Verdana_08_1.aft", "DATA/FONT/Verdana_08_2_bold.aft"})
    if (const auto* font = graph_package.Resolve(path)) font_source_bytes += font->data_size;
  const auto scrollbar_bytes = EstimatedImageBytes(*scroll) +
      EstimatedImageBytes(*panel->VerticalBar());
  const auto real_bytes = EstimatedImageBytes(*showcase_node) + font_source_bytes;
  const auto peak_estimate = real_bytes + scrollbar_bytes + graph_bytes.size() +
      static_cast<std::size_t>(1280 * 720 * 4);
  std::printf("M26 REAL MEMORY graphbuf=%zu scrollbar_est=%zu edit_font_sources=%zu "
              "subtree_est=%zu peak_ui_est=%zu\n",
              graph_bytes.size(), scrollbar_bytes, font_source_bytes, real_bytes,
              peak_estimate);
  std::printf("M26 REAL GRAPHBUF scaled_bytes=%zu frame=%08x/%016llx\n",
      graph->Buffer().bytes(), graph_frame.crc32,
      static_cast<unsigned long long>(graph_frame.fnv64));
  std::vector<std::uint16_t> pixels(1280 * 720);
  scene_compositor::Framebuffer target{pixels.data(), 1280, 720, 1280};
  Check(tree.Render(target, &error), error);
  ui_fingerprint::Value frame{};
  Check(ui_fingerprint::ComputeFramebuffer(target, &frame, &error), error);
  Check(frame.crc32 == 0xe28dc3bcu &&
        frame.fnv64 == UINT64_C(0xae41399ead94b351),
        "M26 fixed real scroll controls host frame");
  std::printf("M26 REAL SCROLL frame=%08x/%016llx up=%dx%d vbar=%dx%d\n",
      frame.crc32, static_cast<unsigned long long>(frame.fnv64),
      scroll->Image(ui::ScrollBarPart::Up, ui::ScrollBarState::Normal)->ClientSize().width,
      scroll->Image(ui::ScrollBarPart::Up, ui::ScrollBarState::Normal)->ClientSize().height,
      panel->VerticalBar()->ClientSize().width, panel->VerticalBar()->ClientSize().height);
  std::puts("M26 REAL SCROLL CONTROLS PASS");
  } catch (const pas::Raised& raised) {
    auto* exception = pas::class_cast_if<pas::Exception*>(raised.object.get());
    std::fprintf(stderr, "M26 REAL SCROLL Delphi exception: %s\n",
        exception ? exception->message.c_str() : "unknown");
    return 1;
  }
}

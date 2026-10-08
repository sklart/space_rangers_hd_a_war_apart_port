#include "ec_file_adapter.hpp"
#include "image_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_object.hpp"
#include "ui_config.hpp"
#include "ui_cache_resolver.hpp"
#include "ui_gai.hpp"
#include "ui_graph_button.hpp"
#include "ui_window.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform;
void Check(bool condition, const std::string& message) {
  if (!condition) { std::fprintf(stderr, "M25 REAL UI FAIL: %s\n", message.c_str()); std::exit(1); }
}
EC_BlockPar::TBlockParEC* Child(EC_BlockPar::TBlockParEC* parent,
                               const std::string& name, int occurrence) {
  if (!parent) return nullptr;
  for (int index = 0, seen = 0; index < parent->GetBlockCount(); ++index) {
    auto candidate = parent->GetBlockNameByIndex(index);
    if (candidate == pas::WideString(name.c_str()) && seen++ == occurrence)
      return parent->GetBlockByIndex(index);
  }
  return nullptr;
}
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string error;
  std::fprintf(stderr, "M25 REAL UI: init\n");
  srhd_awa::platform::ec_file::SetGameRoot(argv[1]);
  Check(aPacket::InitializePackageCollection(), "package collection initialization");
  GR_Main::InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(const_cast<char16_t*>(u"install.txt"), false);
  GR_Main::SelectedLanguage = u"russian"_w;
  GR_Main::LoadLanguageAndPackages();
  std::fprintf(stderr, "M25 REAL UI: load Main.dat\n");
  auto* config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  try { config->LoadFromEncryptedDatFile(u"CFG/Main.dat"); }
  catch (const pas::Raised& raised) {
    auto* exception = pas::class_cast_if<pas::Exception*>(raised.object.get());
    std::fprintf(stderr, "M25 REAL UI: Main.dat load exception: %s\n",
                 exception ? exception->message.c_str() : "unknown");
    return 1;
  }
  std::fprintf(stderr, "M25 REAL UI: select path\n");
  auto* selected = config;
  for (const auto& part : std::array<std::pair<const char*, int>, 7>{{
           {"ML", 0}, {"AB", 0}, {"Panel", 0}, {"Panel", 0},
           {"Panel", 1}, {"Panel", 0}, {"Panel", 0}}}) {
    selected = Child(selected, part.first, part.second);
    Check(selected != nullptr, "selected Main.dat path not found");
  }
  Check(selected->CountParams(u"Name") == 1 && selected->GetParam(u"Name") == u"PLBar",
        "selected Main.dat subtree changed");
  std::fprintf(stderr, "M25 REAL UI: CacheData.dat\n");
  auto* cache_data = pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
  cache_data->LoadFromEncryptedDatFile(u"CFG/CacheData.dat");
  auto* language = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  language->LoadFromEncryptedDatFile(u"CFG/Rus/Lang.dat");
  ui_cache_resolver::CacheUiResourceResolver resolver(cache_data);
  ui::UiTree tree;
  tree.SetRootSize({321, 37});
  auto panel = std::make_unique<ui::UiPanel>();
  std::fprintf(stderr, "M25 REAL UI: factory\n");
  ui_config::Context context{};
  context.resources = &resolver;
  context.styles = Child(config, "ML", 0);
  context.language = language;
  auto* depth_config = config->FindBlock(u"ZPos");
  context.resolve_depth = [depth_config](const std::string& name, double* value) {
    return ui_config::ResolveRuntimeDepth(depth_config, name, value);
  };
  Check(ui_config::ApplyBaseProperties(panel.get(), selected, context, &error), error);
  Check(panel->ClientSize() == ui::Size{321, 37}, "Main.dat panel size changed");
  panel->SetPosition({0, 0}); // Normalize the selected real subtree to its local canvas.
  panel->SetDepth(0);
  Check(ui_config::LoadChildren(panel.get(), selected, context,
                                ui_config::LoadMode::Strict, nullptr, &error), error);
  Check(panel->ChildCount() == 17, "Main.dat image count changed");
  auto* subtree = panel.get();
  Check(tree.Root()->Attach(std::move(panel), &error), error);
  std::vector<std::uint16_t> pixels(321 * 37, 0);
  const scene_compositor::Framebuffer framebuffer{pixels.data(), 321, 37, 321};
  Check(tree.Render(framebuffer, &error), error);
  ui_fingerprint::Value tree_hash{}, frame_hash{};
  Check(ui_fingerprint::ComputeTree(*subtree, &tree_hash, &error), error);
  Check(ui_fingerprint::ComputeFramebuffer(framebuffer, &frame_hash, &error), error);
  std::printf("M25 REAL UI tree=%08x/%016llx frame=%08x/%016llx\n",
              tree_hash.crc32, static_cast<unsigned long long>(tree_hash.fnv64),
              frame_hash.crc32, static_cast<unsigned long long>(frame_hash.fnv64));
  Check(tree_hash.crc32 == 0x73a25b4cu && tree_hash.fnv64 == 0x26d6a269e96b959bull,
        "independent Python tree oracle mismatch");
  Check(frame_hash.crc32 == 0x9cec8dc2u && frame_hash.fnv64 == 0x39c2ccfd0deb5fbbull,
        "independent Python RGB565 oracle mismatch");
  auto* weapon_group = config;
  for (const auto& part : std::array<std::pair<const char*, int>, 6>{{
           {"ML", 0}, {"AB", 0}, {"Panel", 0}, {"Panel", 0},
           {"Panel", 1}, {"Panel", 1}}}) {
    weapon_group = Child(weapon_group, part.first, part.second);
    Check(weapon_group != nullptr, "release metadata parent path changed");
  }
  auto* f1_source = Child(weapon_group, "GraphButton", 0);
  auto* help_source = Child(weapon_group, "GraphButton", 5);
  Check(f1_source && help_source && f1_source->GetParam(u"MVUpdate") == u"True" &&
        f1_source->GetParam(u"MouseBlocking") == u"True" &&
        f1_source->GetParam(u"MouseBlockingTest") == u"False" &&
        help_source->GetParam(u"Help") == u"Help.ButAI",
        "release metadata source values changed");
  auto* f1_config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  f1_config->AddChildBlock(u"GraphButton")->CopyFrom(f1_source);
  ui::UiTree f1_tree;
  f1_tree.SetRootSize({53, 42});
  Check(ui_config::LoadChildren(f1_tree.Root(), f1_config, context,
                                ui_config::LoadMode::Strict, nullptr, &error), error);
  auto* f1 = dynamic_cast<ui::UiGraphButton*>(f1_tree.Root()->FindByNameRecursive("F1"));
  Check(f1 && f1->MouseViewUpdates() && f1->MouseBlocking() && !f1->MouseBlockingTest(),
        "release F1 metadata was not retained");
  f1->SetPosition({0, 0});
  std::array<std::uint16_t, 53 * 42> f1_pixels{};
  const scene_compositor::Framebuffer f1_frame{f1_pixels.data(), 53, 42, 53};
  ui_fingerprint::Value f1_hash{};
  Check(f1_tree.Render(f1_frame, &error) &&
        ui_fingerprint::ComputeFramebuffer(f1_frame, &f1_hash, &error), error);
  Check(f1_hash.crc32 == 0x1c585c8fu && f1_hash.fnv64 == 0x7f23dbb7c1d8df15ull,
        "release F1 metadata changed its normal GI frame");
  pas::free(f1_config);
  auto* window_group = Child(Child(Child(Child(Child(config, "ML", 0), "AB", 0),
                                                "Panel", 0), "Panel", 0), "Panel", 1);
  auto* window_source = window_group ? Child(window_group, "Window", 0) : nullptr;
  Check(window_source && window_source->GetParam(u"Name") == u"InfoPanel" &&
        window_source->GetParam(u"Style") == u"Style.Window.2Simple",
        "release Window source changed");
  auto* window_config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  auto* window_copy = window_config->AddChildBlock(u"Window");
  window_copy->CopyFrom(window_source);
  // Isolate the release border: InfoPanel's GraphBuf child remains outside M25.
  while (window_copy->GetBlockCount() > 0)
    window_copy->DeleteChildBlock(window_copy->GetBlockNameByIndex(0));
  ui::UiTree window_tree;
  window_tree.SetRootSize({282, 175});
  Check(ui_config::LoadChildren(window_tree.Root(), window_config, context,
                                ui_config::LoadMode::Strict, nullptr, &error), error);
  auto* window = dynamic_cast<ui::UiWindow*>(window_tree.Root()->FindByNameRecursive("InfoPanel"));
  Check(window && !window->MouseBlocking() && window->MouseViewUpdates() &&
        window->ClientSize() == ui::Size{282, 175},
        "release Window geometry or metadata changed");
  window->SetActive(true); window->SetPosition({0, 0}); window->SetDepth(0);
  std::vector<std::uint16_t> window_pixels(282 * 175, 0);
  const scene_compositor::Framebuffer window_frame{window_pixels.data(), 282, 175, 282};
  ui_fingerprint::Value window_hash{};
  Check(window_tree.Render(window_frame, &error) &&
        ui_fingerprint::ComputeFramebuffer(window_frame, &window_hash, &error), error);
  Check(window_hash.crc32 == 0x87e5a68fu && window_hash.fnv64 == 0x8020187c43bbcc26ull,
        "release Window style border differs from Python oracle");
  pas::free(window_config);
  ui_fingerprint::Value help_hashes[2]{};
  for (int without_help = 0; without_help != 2; ++without_help) {
    auto* button_config = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
    auto* copied = button_config->AddChildBlock(u"GraphButton");
    copied->CopyFrom(help_source);
    if (without_help) copied->DeleteParam(u"Help");
    ui::UiTree help_tree;
    help_tree.SetRootSize({46, 33});
    Check(ui_config::LoadChildren(help_tree.Root(), button_config, context,
                                  ui_config::LoadMode::Strict, nullptr, &error), error);
    auto* help_button = dynamic_cast<ui::UiGraphButton*>(help_tree.Root()->FindByNameRecursive("ButAuto"));
    Check(help_button != nullptr, "release Help GraphButton did not load");
    if (!without_help)
      Check(help_button->HelpKey() == "Help.ButAI" &&
            help_button->HelpText().find("<color=255,245,80>A</color>") != std::string::npos,
            "release Help localization was not retained");
    help_button->SetPosition({0, 0});
    std::array<std::uint16_t, 46 * 33> help_pixels{};
    const scene_compositor::Framebuffer help_frame{help_pixels.data(), 46, 33, 46};
    Check(help_tree.Render(help_frame, &error) &&
          ui_fingerprint::ComputeFramebuffer(help_frame, &help_hashes[without_help], &error), error);
    pas::free(button_config);
  }
  Check(help_hashes[0].crc32 == help_hashes[1].crc32 &&
        help_hashes[0].fnv64 == help_hashes[1].fnv64,
        "release Help metadata changed the visual frame");
  ui::UiGaiLeaf gai;
  Check(resolver.LoadGai(&gai, "Bm.PI.PathEndMove", &error) &&
        gai.SelectEmbeddedSequence(0, &error), error);
  std::array<std::uint16_t, 32 * 32> gai_pixels{};
  const scene_compositor::Framebuffer gai_frame{gai_pixels.data(), 32, 32, 32};
  ui_fingerprint::Value gai_hash{};
  Check(gai.Render(gai_frame, {0, 0, 32, 32}, &error) &&
        ui_fingerprint::ComputeFramebuffer(gai_frame, &gai_hash, &error), error);
  Check(gai_hash.crc32 == 0x71b457cdu && gai_hash.fnv64 == 0xd75029d766f2bdedull,
        "CacheData GAI frame differs from Python oracle");
  pas::free(config);
  pas::free(cache_data);
  pas::free(language);
  pas::free(GR_Main::InstallConfig);
  pas::free(GR_Main::LanguageInstallConfig);
  aPacket::FinalizePackageCollection();
  std::puts("M25 REAL UI PASS");
}

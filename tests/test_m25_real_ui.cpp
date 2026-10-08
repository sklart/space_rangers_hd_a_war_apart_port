#include "ec_file_adapter.hpp"
#include "image_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_object.hpp"
#include "ui_config.hpp"
#include "ui_cache_resolver.hpp"
#include "ui_gai.hpp"
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
  ui_cache_resolver::CacheUiResourceResolver resolver(cache_data);
  ui::UiTree tree;
  tree.SetRootSize({321, 37});
  auto panel = std::make_unique<ui::UiPanel>();
  std::fprintf(stderr, "M25 REAL UI: factory\n");
  ui_config::Context context{};
  context.resources = &resolver;
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
  pas::free(GR_Main::InstallConfig);
  pas::free(GR_Main::LanguageInstallConfig);
  aPacket::FinalizePackageCollection();
  std::puts("M25 REAL UI PASS");
}

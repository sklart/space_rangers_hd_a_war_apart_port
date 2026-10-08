#include "ec_file_adapter.hpp"
#include "image_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_object.hpp"
#include "ui_config.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_tree_renderer.hpp"
#include "units/EC_BlockPar.hpp"
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
class ReleaseResolver final : public ui_config::IUiResourceResolver {
 public:
  explicit ReleaseResolver(srhd_awa::package::Package* package) : package_(package) {}
  bool LoadImage(ui::UiImageLeaf* leaf, image_object::Kind kind,
                 const std::string& resource, const std::string& option,
                 std::string* error) override {
    if (!leaf || kind != image_object::Kind::GI || !option.empty()) return false;
    std::string path;
    if (resource == "Bm.FormLoad2.2BarLeft") path = "DATA/FormLoad2/2BarLeft.gi";
    else if (resource == "Bm.FormLoad2.2BarCenter") path = "DATA/FormLoad2/2BarCenter.gi";
    else if (resource == "Bm.FormLoad2.2BarRight") path = "DATA/FormLoad2/2BarRight.gi";
    else { if (error) *error = "unexpected Main.dat GI resource: " + resource; return false; }
    const auto* entry = package_->Resolve(path);
    if (!entry) { if (error) *error = "missing package resource: " + path; return false; }
    std::vector<std::uint8_t> bytes;
    return package_->ReadPayload(*entry, &bytes, error) &&
           leaf->LoadBytes(kind, bytes.data(), bytes.size(), resource, option, error);
  }
 private:
  srhd_awa::package::Package* package_{};
};
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string error;
  std::fprintf(stderr, "M25 REAL UI: init\n");
  srhd_awa::platform::ec_file::SetGameRoot(argv[1]);
  Check(aPacket::InitializePackageCollection(), "package collection initialization");
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
  srhd_awa::package::Package package;
  std::fprintf(stderr, "M25 REAL UI: package\n");
  Check(package.Open(std::string(argv[1]) + "/DATA/forms.pkg", &error), error);
  ReleaseResolver resolver(&package);
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
  pas::free(config);
  aPacket::FinalizePackageCollection();
  std::puts("M25 REAL UI PASS");
}

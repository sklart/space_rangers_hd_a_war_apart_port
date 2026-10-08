#include "ui_config.hpp"
#include "ui_object.hpp"
#include "ui_tree_renderer.hpp"
#include "units/EC_BlockPar.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void Check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
EC_BlockPar::TBlockParEC* Block() { return pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create); }
void Param(EC_BlockPar::TBlockParEC* block, const char16_t* name, const char16_t* value) { block->AddParam(name, value); }
class RecordingResolver final : public srhd_awa::platform::ui_config::IUiResourceResolver {
 public:
  struct Call { srhd_awa::platform::image_object::Kind kind; std::string resource; std::string option; };
  bool LoadImage(srhd_awa::platform::ui::UiImageLeaf* leaf, srhd_awa::platform::image_object::Kind kind,
                 const std::string& resource, const std::string& option, std::string*) override {
    calls.push_back({kind, resource, option}); return leaf != nullptr;
  }
  std::vector<Call> calls;
};
}

int main() {
  auto* styles = Block(); auto* base = styles->AddChildBlock(u"Base");
  Param(base, u"Pos", u"10,20,Back"); Param(base, u"Size", u"100,50"); Param(base, u"Sme", u"3,4");
  auto* depth = Block(); Param(depth, u"Front", u"-10"); Param(depth, u"Back", u"100");
  auto* config = Block(); Param(config, u"Style", u"Base"); Param(config, u"Pos", u"30,40,Front,w"); Param(config, u"Name", u"local"); Param(config, u"Active", u"False");
  srhd_awa::platform::ui::UiObject object; srhd_awa::platform::ui_config::Context context{styles, [depth](const std::string& name, double* value) { return srhd_awa::platform::ui_config::ResolveRuntimeDepth(depth, name, value); }};
  std::string error; Check(srhd_awa::platform::ui_config::ApplyBaseProperties(&object, config, context, &error), error.c_str());
  Check(object.LocalPosition() == srhd_awa::platform::ui::Point{30, 40}, "local Pos overrides style");
  Check(object.ClientSize() == srhd_awa::platform::ui::Size{100, 50}, "style Size retained");
  Check(object.Origin() == srhd_awa::platform::ui::Point{3, 4}, "Sme origin");
  Check(object.Depth() == -10 && object.PositionModeW(), "symbolic depth and w");
  Check(object.Name() == "local" && !object.Active(), "Name and exact Active=False");
  config->SetParam(u"Active", u"false"); Check(srhd_awa::platform::ui_config::ApplyBaseProperties(&object, config, context, &error) && object.Active(), "only exact False disables");
  auto* cyclic = Block(); auto* a = cyclic->AddChildBlock(u"A"); auto* b = cyclic->AddChildBlock(u"B"); Param(a, u"Style", u"B"); Param(b, u"Style", u"A"); auto* cycle_config = Block(); Param(cycle_config, u"Style", u"A");
  context.styles = cyclic; Check(!srhd_awa::platform::ui_config::ApplyBaseProperties(&object, cycle_config, context, &error), "Style cycle rejected");
  auto* tree_config = Block(); auto* panel = tree_config->AddChildBlock(u"Panel"); Param(panel, u"Pos", u"5,6");
  auto* simple = panel->AddChildBlock(u"SimpleImage"); Param(simple, u"Image", u"simple.bmp"); Param(simple, u"KindX", u"Left"); Param(simple, u"KindY", u"Bottom");
  auto* decorations = panel->AddChildBlock(u"Decorations"); auto* alpha = decorations->AddChildBlock(u"AlphaImage"); Param(alpha, u"Image", u"alpha.bmp"); Param(alpha, u"AlignX", u"Right"); Param(alpha, u"AlignY", u"Top");
  auto* generic = panel->AddChildBlock(u"Image"); Param(generic, u"Image", u"Trans, generic.bmp"); Param(generic, u"HalfAlpha", u"True");
  panel->AddChildBlock(u"OnPressCode"); auto* unsupported = tree_config->AddChildBlock(u"Label"); unsupported->AddChildBlock(u"SimpleImage");
  RecordingResolver resolver; context.styles = nullptr; context.resources = &resolver; srhd_awa::platform::ui::UiTree tree; srhd_awa::platform::ui_config::LoadReport report;
  Check(srhd_awa::platform::ui_config::LoadChildren(tree.Root(), tree_config, context, srhd_awa::platform::ui_config::LoadMode::Inventory, &report, &error), error.c_str());
  Check(tree.Root()->ChildCount() == 1 && tree.Root()->Children()[0]->Kind() == srhd_awa::platform::ui::NodeKind::Panel, "Panel factory and inventory skip");
  auto* built_panel = static_cast<srhd_awa::platform::ui::UiPanel*>(tree.Root()->Children()[0].get()); Check(built_panel->LocalPosition() == srhd_awa::platform::ui::Point{5, 6}, "factory base properties");
  Check(built_panel->ChildCount() == 3 && report.unsupported_controls.size() == 1 && report.unsupported_controls[0] == "Label" && report.skipped_events.size() == 1, "grouping, unsupported and event handling");
  auto* built_generic = static_cast<srhd_awa::platform::ui::UiImageLeaf*>(built_panel->Children()[0].get()); auto* built_alpha = static_cast<srhd_awa::platform::ui::UiImageLeaf*>(built_panel->Children()[1].get()); auto* built_simple = static_cast<srhd_awa::platform::ui::UiImageLeaf*>(built_panel->Children()[2].get());
  Check(built_simple->Image().x_mode() == srhd_awa::platform::image_layout::XMode::Left && built_simple->Image().y_mode() == srhd_awa::platform::image_layout::YMode::Bottom, "Kind image layout");
  Check(built_alpha->Image().x_mode() == srhd_awa::platform::image_layout::XMode::Right && built_alpha->Image().y_mode() == srhd_awa::platform::image_layout::YMode::Top, "Align image layout");
  Check(built_generic->Image().half_alpha(), "generic Image half alpha");
  Check(resolver.calls.size() == 3 && resolver.calls[0].kind == srhd_awa::platform::image_object::Kind::Simple && resolver.calls[1].kind == srhd_awa::platform::image_object::Kind::Alpha && resolver.calls[2].kind == srhd_awa::platform::image_object::Kind::Trans && resolver.calls[2].resource == "generic.bmp", "image factory kinds and resolver");
  srhd_awa::platform::ui::UiTree strict_tree; Check(!srhd_awa::platform::ui_config::LoadChildren(strict_tree.Root(), tree_config, context, srhd_awa::platform::ui_config::LoadMode::Strict, &report, &error) && error == "unsupported UI control: Label", "strict known unsupported control");
  pas::free(styles); pas::free(depth); pas::free(config); pas::free(cyclic); pas::free(cycle_config); pas::free(tree_config);
  std::cout << "UI CONFIG TEST PASS\n";
}

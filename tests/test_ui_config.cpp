#include "ui_config.hpp"
#include "ui_object.hpp"
#include "units/EC_BlockPar.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void Check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
EC_BlockPar::TBlockParEC* Block() { return pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create); }
void Param(EC_BlockPar::TBlockParEC* block, const char16_t* name, const char16_t* value) { block->AddParam(name, value); }
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
  pas::free(styles); pas::free(depth); pas::free(config); pas::free(cyclic); pas::free(cycle_config);
  std::cout << "UI CONFIG TEST PASS\n";
}

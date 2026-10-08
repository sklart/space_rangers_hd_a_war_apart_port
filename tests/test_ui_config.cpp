#include "ui_config.hpp"
#include "font_repository.hpp"
#include "m23_aft_fixture.hpp"
#include "m24_checkpoint_fixture.hpp"
#include "ui_label.hpp"
#include "ui_object.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_graph_button.hpp"
#include "ui_window.hpp"
#include "ui_zone.hpp"
#include "scene_compositor.hpp"
#include "units/EC_BlockPar.hpp"

#include <algorithm>
#include <array>
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
class TinyResolver final : public srhd_awa::platform::ui_config::IUiResourceResolver {
 public:
  bool LoadImage(srhd_awa::platform::ui::UiImageLeaf* leaf,
                 srhd_awa::platform::image_object::Kind kind,
                 const std::string& resource, const std::string& option,
                 std::string* error) override {
    const auto bytes = srhd_awa::platform::m24_checkpoint_fixture::Bmp(0, 0, 255);
    return leaf && leaf->LoadBytes(kind, bytes.data(), bytes.size(), resource, option, error);
  }
};
class FixtureFontResolver final : public srhd_awa::platform::font_repository::IFontResolver {
 public:
  bool LoadFont(const std::string& key, std::vector<std::uint8_t>* bytes,
                std::string* source, std::string*) override {
    if (key != "Font.fixture") return false;
    ++loads;
    *bytes = m23_test::AftFixture();
    *source = "synthetic";
    return true;
  }
  int loads{};
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
  panel->AddChildBlock(u"OnPressCode"); auto* unsupported = tree_config->AddChildBlock(u"SimpleButton"); unsupported->AddChildBlock(u"SimpleImage");
  RecordingResolver resolver; context.styles = nullptr; context.resources = &resolver; srhd_awa::platform::ui::UiTree tree; srhd_awa::platform::ui_config::LoadReport report;
  Check(srhd_awa::platform::ui_config::LoadChildren(tree.Root(), tree_config, context, srhd_awa::platform::ui_config::LoadMode::Inventory, &report, &error), error.c_str());
  Check(tree.Root()->ChildCount() == 1 && tree.Root()->Children()[0]->Kind() == srhd_awa::platform::ui::NodeKind::Panel, "Panel factory and inventory skip");
  auto* built_panel = static_cast<srhd_awa::platform::ui::UiPanel*>(tree.Root()->Children()[0].get()); Check(built_panel->LocalPosition() == srhd_awa::platform::ui::Point{5, 6}, "factory base properties");
  Check(built_panel->ChildCount() == 3 && report.unsupported_controls.size() == 1 && report.unsupported_controls[0] == "SimpleButton" && report.skipped_events.size() == 1, "grouping, unsupported and event handling");
  auto* built_generic = static_cast<srhd_awa::platform::ui::UiImageLeaf*>(built_panel->Children()[0].get()); auto* built_alpha = static_cast<srhd_awa::platform::ui::UiImageLeaf*>(built_panel->Children()[1].get()); auto* built_simple = static_cast<srhd_awa::platform::ui::UiImageLeaf*>(built_panel->Children()[2].get());
  Check(built_simple->Image().x_mode() == srhd_awa::platform::image_layout::XMode::Left && built_simple->Image().y_mode() == srhd_awa::platform::image_layout::YMode::Bottom, "Kind image layout");
  Check(built_alpha->Image().x_mode() == srhd_awa::platform::image_layout::XMode::Right && built_alpha->Image().y_mode() == srhd_awa::platform::image_layout::YMode::Top, "Align image layout");
  Check(built_generic->Image().half_alpha(), "generic Image half alpha");
  Check(resolver.calls.size() == 3 && resolver.calls[0].kind == srhd_awa::platform::image_object::Kind::Simple && resolver.calls[1].kind == srhd_awa::platform::image_object::Kind::Alpha && resolver.calls[2].kind == srhd_awa::platform::image_object::Kind::Trans && resolver.calls[2].resource == "generic.bmp", "image factory kinds and resolver");
  srhd_awa::platform::ui::UiTree strict_tree; Check(!srhd_awa::platform::ui_config::LoadChildren(strict_tree.Root(), tree_config, context, srhd_awa::platform::ui_config::LoadMode::Strict, &report, &error) && error == "unsupported UI control: SimpleButton", "strict known unsupported control");
  auto* zone_styles = Block(); auto* zone_style = zone_styles->AddChildBlock(u"ZoneBase");
  Param(zone_style, u"Kind", u"Circle"); Param(zone_style, u"Size", u"12,12");
  auto* zone_config = Block(); auto* zone_block = zone_config->AddChildBlock(u"Zone");
  Param(zone_block, u"Style", u"ZoneBase"); Param(zone_block, u"Kind", u"Rect");
  Param(zone_block, u"Pos", u"3,4");
  srhd_awa::platform::ui::UiTree zone_tree;
  auto zone_context = context; zone_context.styles = zone_styles;
  Check(srhd_awa::platform::ui_config::LoadChildren(zone_tree.Root(), zone_config, zone_context,
      srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error), error.c_str());
  auto* built_zone = dynamic_cast<srhd_awa::platform::ui::UiZone*>(zone_tree.Root()->Children()[0].get());
  Check(built_zone && built_zone->GetZoneKind() == srhd_awa::platform::ui::ZoneKind::Rect &&
      built_zone->ClientSize() == srhd_awa::platform::ui::Size{12, 12} &&
      built_zone->HitTest({3, 4}), "Zone style inheritance and local Kind override");
  auto* button_config = Block(); auto* button_block = button_config->AddChildBlock(u"GraphButton");
  Param(button_block, u"Pos", u"11,7"); Param(button_block, u"Size", u"20,9");
  Param(button_block, u"Kind", u"FixDisable"); Param(button_block, u"Down", u"True");
  Param(button_block, u"ImageNormal", u"Simple,normal.bmp");
  Param(button_block, u"ImageNormalA", u"Simple,normal-a.bmp");
  Param(button_block, u"ImageDown", u"Trans,down.bmp");
  Param(button_block, u"ImageDownA", u"Trans,down-a.bmp");
  Param(button_block, u"ImageDisable", u"Simple,disabled.bmp");
  Param(button_block, u"ImageDisableA", u"Simple,disabled-a.bmp");
  Param(button_block, u"ImageDown_Pos", u"14,9");
  Param(button_block, u"SoundClick", u"click-id");
  button_block->AddChildBlock(u"OnPressCode");
  srhd_awa::platform::ui::UiTree button_tree;
  Check(srhd_awa::platform::ui_config::LoadChildren(button_tree.Root(), button_config, context,
      srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error), error.c_str());
  auto* built_button = dynamic_cast<srhd_awa::platform::ui::UiGraphButton*>(button_tree.Root()->Children()[0].get());
  Check(built_button && built_button->VisualSlot() == srhd_awa::platform::ui::GraphButtonSlot::Down &&
      built_button->StateOffset(srhd_awa::platform::ui::GraphButtonSlot::Down) ==
          srhd_awa::platform::ui::Point{3, 2} && built_button->HasOnPressCode() &&
      built_button->SoundClick() == "click-id", "GraphButton config modes, offset and metadata");
  built_button->SetHovered(true);
  Check(built_button->VisualSlot() == srhd_awa::platform::ui::GraphButtonSlot::DownA &&
      built_button->StateImage(srhd_awa::platform::ui::GraphButtonSlot::DownA)->Active(),
      "GraphButton down hovered state");
  built_button->SetDisabled(true);
  Check(built_button->VisualSlot() == srhd_awa::platform::ui::GraphButtonSlot::DisableA &&
      built_button->StateImage(srhd_awa::platform::ui::GraphButtonSlot::DisableA)->Active(),
      "GraphButton disabled hovered priority");
  built_button->SetButtonKind(srhd_awa::platform::ui::GraphButtonKind::Fix);
  Check(built_button->VisualSlot() == srhd_awa::platform::ui::GraphButtonSlot::DownA,
      "GraphButton Fix ignores disabled visual");
  built_button->SetDown(false);
  Check(built_button->VisualSlot() == srhd_awa::platform::ui::GraphButtonSlot::NormalA,
      "GraphButton normal hovered state");
  button_block->SetParam(u"ImageNormal", u"GI,normal.bmp");
  srhd_awa::platform::ui::UiTree rejected_button;
  Check(!srhd_awa::platform::ui_config::LoadChildren(rejected_button.Root(), button_config, context,
      srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error) &&
      error == "unsupported generic Image mode", "GraphButton GI mode stays unsupported");
  auto* window_config = Block(); auto* window_block = window_config->AddChildBlock(u"Window");
  Param(window_block, u"ImageTopLeft", u"GI,border.gi");
  srhd_awa::platform::ui::UiTree rejected_window;
  Check(!srhd_awa::platform::ui_config::LoadChildren(rejected_window.Root(), window_config,
      context, srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error) &&
      error == "unsupported generic Image mode", "Window GI mode stays unsupported");
  srhd_awa::platform::ui::UiTree inventoried_window;
  srhd_awa::platform::ui_config::LoadReport window_report;
  Check(srhd_awa::platform::ui_config::LoadChildren(inventoried_window.Root(), window_config,
      context, srhd_awa::platform::ui_config::LoadMode::Inventory, &window_report, &error) &&
      inventoried_window.Root()->ChildCount() == 0 &&
      window_report.unsupported_controls == std::vector<std::string>{"Window"},
      "Window unsupported release mode is inventoried without substitution");
  auto* window_styles = Block(); auto* border_style = window_styles->AddChildBlock(u"BorderStyle");
  for (const auto* slot : {u"ImageTopLeft", u"ImageTopRight", u"ImageBottomLeft",
                           u"ImageBottomRight", u"ImageLeft", u"ImageRight",
                           u"ImageTop", u"ImageBottom", u"ImageTexture"})
    Param(border_style, slot, u"Simple,border.bmp");
  Param(border_style, u"MinSize", u"8,6");
  Param(border_style, u"WorkSubRect", u"1,1,7,5");
  auto* supported_window_config = Block();
  auto* supported_window_block = supported_window_config->AddChildBlock(u"Window");
  Param(supported_window_block, u"Style", u"BorderStyle");
  Param(supported_window_block, u"Size", u"7,5");
  auto* child_zone = supported_window_block->AddChildBlock(u"Zone");
  Param(child_zone, u"Kind", u"Circle"); Param(child_zone, u"Size", u"3,3");
  Param(child_zone, u"Name", u"inner-zone");
  TinyResolver tiny_resolver;
  auto supported_window_context = context;
  supported_window_context.styles = window_styles;
  supported_window_context.resources = &tiny_resolver;
  srhd_awa::platform::ui::UiTree supported_window_tree;
  Check(srhd_awa::platform::ui_config::LoadChildren(supported_window_tree.Root(),
      supported_window_config, supported_window_context,
      srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error), error.c_str());
  auto* built_window = dynamic_cast<srhd_awa::platform::ui::UiWindow*>(
      supported_window_tree.Root()->Children()[0].get());
  Check(built_window && built_window->ClientSize() == srhd_awa::platform::ui::Size{8, 6} &&
      built_window->WorkSubRect() == srhd_awa::platform::ui::Rect{1, 1, 7, 5} &&
      built_window->ChildCount() == 10 &&
      built_window->BorderImage(srhd_awa::platform::ui::WindowSlot::Texture)->Active() &&
      built_window->FindByNameRecursive("inner-zone") &&
      built_window->FindByNameRecursive("inner-zone")->Kind() ==
          srhd_awa::platform::ui::NodeKind::Zone,
      "Window style, nine images, nested Zone and tile-aligned size");
  auto* label_styles = Block(); auto* label_style = label_styles->AddChildBlock(u"BaseLabel");
  Param(label_style, u"Font", u"Font.fixture"); Param(label_style, u"TextColor", u"255,0,0");
  Param(label_style, u"AlignX", u"Right"); Param(label_style, u"AlignY", u"CenterEx");
  Param(label_style, u"Size", u"40,20");
  auto* label_config = Block(); auto* repeated = label_config->AddChildBlock(u"Label");
  Param(repeated, u"Name", u"repeated");
  Param(repeated, u"Style", u"BaseLabel"); Param(repeated, u"Text", u"A");
  Param(repeated, u"Text", u"B"); Param(repeated, u"AlignX", u"Left");
  Param(repeated, u"Size", u"50,24");
  auto* localized = label_config->AddChildBlock(u"Label");
  Param(localized, u"Name", u"localized");
  Param(localized, u"Style", u"BaseLabel"); Param(localized, u"Text", u"Greeting.Title");
  auto* language = Block(); auto* greeting = language->AddChildBlock(u"Greeting");
  Param(greeting, u"Title", u"AB");
  FixtureFontResolver font_resolver;
  srhd_awa::platform::font_repository::Repository fonts(&font_resolver);
  srhd_awa::platform::ui_config::Context label_context{};
  label_context.styles = label_styles; label_context.fonts = &fonts; label_context.language = language;
  auto* caption_style = label_styles->AddChildBlock(u"ButtonCaption");
  Param(caption_style, u"Font", u"Font.fixture");
  Param(caption_style, u"Caption", u"Greeting.Title");
  Param(caption_style, u"CaptionColor", u"0,255,0");
  Param(caption_style, u"CaptionColorNormalA", u"255,0,0");
  Param(caption_style, u"CaptionShadowColorNormalA", u"0,0,255");
  Param(caption_style, u"CaptionShadow", u"1");
  Param(caption_style, u"CaptionSme", u"1,2,3,4");
  auto* caption_config = Block(); auto* caption_button = caption_config->AddChildBlock(u"GraphButton");
  Param(caption_button, u"Style", u"ButtonCaption"); Param(caption_button, u"Size", u"20,16");
  Param(caption_button, u"ImageNormal", u"Simple,normal.bmp");
  Param(caption_button, u"ImageNormalA", u"Simple,normal-a.bmp");
  Param(caption_button, u"CaptionColorNormalA", u"255,255,0");
  label_context.resources = &resolver;
  srhd_awa::platform::ui::UiTree caption_tree;
  Check(srhd_awa::platform::ui_config::LoadChildren(caption_tree.Root(), caption_config,
      label_context, srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error), error.c_str());
  auto* caption_button_object = dynamic_cast<srhd_awa::platform::ui::UiGraphButton*>(caption_tree.Root()->Children()[0].get());
  Check(caption_button_object && caption_button_object->Caption() &&
      caption_button_object->Caption()->TextLines() == std::vector<std::u16string>{u"AB"} &&
      caption_button_object->Caption()->TextColor() == 0x07e0 &&
      caption_button_object->Caption()->LocalPosition() == srhd_awa::platform::ui::Point{1, 2},
      "GraphButton caption font, localization, default state and normal offset");
  caption_button_object->SetHovered(true);
  Check(caption_button_object->Caption()->TextColor() == 0xffe0 &&
      caption_button_object->Caption()->TextShadowOffset() == 1 &&
      caption_button_object->Caption()->TextShadowColor() == 0x001f,
      "GraphButton local state color and shadow");
  caption_button_object->SetDown(true);
  Check(caption_button_object->Caption()->LocalPosition() == srhd_awa::platform::ui::Point{3, 4},
      "GraphButton down caption offset");
  srhd_awa::platform::ui::UiTree label_tree;
  Check(srhd_awa::platform::ui_config::LoadChildren(label_tree.Root(), label_config,
      label_context, srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, &error), error.c_str());
  Check(label_tree.Root()->ChildCount() == 2, "Label factory creates both labels");
  auto* first_label = static_cast<srhd_awa::platform::ui::UiLabelLeaf*>(label_tree.Root()->FindByNameRecursive("repeated"));
  auto* second_label = static_cast<srhd_awa::platform::ui::UiLabelLeaf*>(label_tree.Root()->FindByNameRecursive("localized"));
  Check(first_label && second_label, "Label names resolve after depth ordering");
  Check(first_label->Kind() == srhd_awa::platform::ui::NodeKind::LabelLeaf,
      "Label factory kind");
  Check(first_label->FontKey() == "Font.fixture", "Label inherited font");
  Check(first_label->TextColor() == 0xf800, "Label inherited color");
  Check(first_label->AlignX() == srhd_awa::platform::ui::LabelAlignX::Left,
      "Label local X alignment");
  Check(first_label->AlignY() == srhd_awa::platform::ui::LabelAlignY::CenterEx,
      "Label inherited Y alignment");
  Check(first_label->ClientSize() == srhd_awa::platform::ui::Size{50, 24},
      "Label local size override");
  Check(first_label->TextLines() == std::vector<std::u16string>{u"A", u"B"},
      "repeated Label Text preserves order");
  Check(second_label->TextLines() == std::vector<std::u16string>{u"AB"},
      "Label localization replaces a present key");
  Check(font_resolver.loads == 1 && fonts.size() == 1, "Label font repository parses once");
  std::shared_ptr<const srhd_awa::platform::aft_font::AftFont> fixture_font;
  Check(fonts.Acquire("Font.fixture", &fixture_font, nullptr, &error), error.c_str());
  srhd_awa::platform::ui::UiTree draw_tree;
  draw_tree.SetRootSize({32, 16});
  auto* drawn = draw_tree.Root()->AddLabel();
  drawn->SetSize({16, 12}); drawn->SetFont("Font.fixture", fixture_font);
  drawn->SetTextLines({u"A"}); drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Left);
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::Top);
  drawn->SetTextColor(0xf800); drawn->SetTextShadow(1, 0x07e0);
  drawn->SetTextBorder(0, 0x001f); drawn->SetBorder(true, 0xffff, 0x39e7);
  draw_tree.UpdateGeometry();
  std::array<std::uint16_t, 32 * 16> pixels{};
  srhd_awa::platform::scene_compositor::Framebuffer target{pixels.data(), 32, 16, 32};
  Check(draw_tree.Render(target, &error), error.c_str());
  Check(pixels[0] == 0xffff && pixels[11 * 32 + 15] == 0x39e7,
      "Label light and dark outer border");
  Check(pixels[3 * 32 + 2] == 0xf800 && pixels[5 * 32 + 4] == 0x07e0,
      "Label main glyph and shadow");
  pixels.fill(0); drawn->SetTextBorder(1, 0x001f);
  Check(draw_tree.Render(target, &error), error.c_str());
  Check(pixels[3 * 32 + 2] == 0xf800 && pixels[5 * 32 + 4] == 0x001f &&
      pixels[2 * 32 + 1] == 0x001f,
      "Label diagonal outline overlays shadow before main glyph");
  pixels.fill(0);
  Check(drawn->Render(target, {0, 0, 3, 4}, &error), error.c_str());
  Check(pixels[5 * 32 + 4] == 0, "Label draw respects half-open clip");
  pixels.fill(0); drawn->SetActive(false);
  Check(draw_tree.Render(target, &error), error.c_str());
  Check(std::all_of(pixels.begin(), pixels.end(), [](auto pixel) { return pixel == 0; }),
      "inactive Label is skipped by tree renderer");
  drawn->SetActive(true); drawn->SetBorder(false, 0xffff, 0x39e7);
  drawn->SetTextShadow(0, 0); drawn->SetTextBorder(0, 0);
  drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Auto);
  drawn->SetTextLines({u"A B", u"ABCA"});
  Check(drawn->Prepare(&error) && drawn->ClientSize().width == 24,
      "Label Auto width is measured width plus four");
  pixels.fill(0);
  Check(draw_tree.Render(target, &error), error.c_str());
  Check(pixels[3 * 32 + 17] != 0 && pixels[3 * 32 + 11] == 0 &&
      pixels[10 * 32 + 7] != 0,
      "Label Auto justifies nonfinal line and leaves final line unexpanded");
  auto first_pixel = [&]() {
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 32; ++x)
      if (pixels[y * 32 + x]) return srhd_awa::platform::ui::Point{x, y};
    return srhd_awa::platform::ui::Point{-1, -1};
  };
  drawn->SetTextLines({u"A"}); drawn->SetSize({20, 16});
  drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Left);
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::Top);
  pixels.fill(0); Check(draw_tree.Render(target, &error), error.c_str());
  Check(first_pixel() == srhd_awa::platform::ui::Point{2, 3}, "Label Left/Top");
  drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Center);
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::Center);
  pixels.fill(0); Check(draw_tree.Render(target, &error), error.c_str());
  Check(first_pixel() == srhd_awa::platform::ui::Point{8, 7}, "Label Center/Center");
  drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Right);
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::Bottom);
  pixels.fill(0); Check(draw_tree.Render(target, &error), error.c_str());
  Check(first_pixel() == srhd_awa::platform::ui::Point{13, 11}, "Label Right/Bottom");
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::CenterEx);
  pixels.fill(0); Check(draw_tree.Render(target, &error), error.c_str());
  Check(first_pixel() == srhd_awa::platform::ui::Point{13, 10}, "Label CenterEx differs from Bottom");
  drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Auto);
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::Auto);
  Check(drawn->Prepare(&error) && drawn->ClientSize() == srhd_awa::platform::ui::Size{9, 8},
      "Label Auto X/Y dimensions");
  drawn->SetSize({14, 16}); drawn->SetAlignX(srhd_awa::platform::ui::LabelAlignX::Left);
  drawn->SetAlignY(srhd_awa::platform::ui::LabelAlignY::Top);
  drawn->SetWordWrap(true); drawn->SetTextLines({u"A B C"});
  Check(drawn->Prepare(&error) && drawn->RenderedLineCount() >= 2,
      "Label WordWrap uses client width minus four");
  drawn->SetWordWrap(false); drawn->SetTextLines({u"\u0401\u044f"});
  pixels.fill(0); Check(draw_tree.Render(target, &error), error.c_str());
  Check(first_pixel().x >= 0, "Label fixture Cyrillic glyphs render");
  pas::free(label_styles); pas::free(label_config); pas::free(language);
  pas::free(styles); pas::free(depth); pas::free(config); pas::free(cyclic); pas::free(cycle_config); pas::free(tree_config);
  std::cout << "UI CONFIG TEST PASS\n";
}

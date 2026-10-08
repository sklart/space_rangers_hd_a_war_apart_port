#include "ui_config.hpp"

#include "image_layout.hpp"
#include "image_object.hpp"
#include "font_repository.hpp"
#include "package.hpp"
#include "ui_label.hpp"
#include "ui_graph_button.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_zone.hpp"
#include "ui_window.hpp"
#include "units/EC_BlockPar.hpp"

#include <charconv>
#include <array>
#include <cstdlib>
#include <memory>
#include <unordered_set>
#include <vector>

namespace srhd_awa::platform::ui_config {
namespace {
bool Fail(std::string* error, const char* message) { if (error) *error = message; return false; }
std::string Text(const pas::WideString& value) { return static_cast<pas::AnsiString>(value).c_str(); }
bool Has(EC_BlockPar::TBlockParEC* block, const char16_t* name) { return block && block->CountParams(name) > 0; }
std::vector<std::string> Split(const std::string& value) {
  std::vector<std::string> parts; std::size_t start{};
  for (std::size_t index{}; index <= value.size(); ++index) if (index == value.size() || value[index] == ',') {
    auto part = value.substr(start, index - start); const auto first = part.find_first_not_of(" \t\r\n");
    const auto last = part.find_last_not_of(" \t\r\n"); parts.push_back(first == std::string::npos ? "" : part.substr(first, last - first + 1)); start = index + 1;
  }
  return parts;
}
bool Number(const std::string& value, std::int32_t* result) {
  if (value.empty()) return false;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), *result);
  return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
bool Decimal(const std::string& value, double* result) {
  char* end{}; *result = std::strtod(value.c_str(), &end); return end && *end == '\0';
}
bool Enabled(const std::string& value) {
  return value == "Yes" || value == "yes" || value == "True" || value == "true" || value == "TRUE" || value == "1";
}
bool XMode(const std::string& value, image_layout::XMode* mode) {
  if (value == "LeftFill") *mode = image_layout::XMode::LeftFill;
  else if (value == "CenterFill") *mode = image_layout::XMode::CenterFill;
  else if (value == "RightFill") *mode = image_layout::XMode::RightFill;
  else if (value == "Left") *mode = image_layout::XMode::Left;
  else if (value == "Center") *mode = image_layout::XMode::Center;
  else if (value == "Right") *mode = image_layout::XMode::Right;
  else return false;
  return true;
}
bool YMode(const std::string& value, image_layout::YMode* mode) {
  if (value == "TopFill") *mode = image_layout::YMode::TopFill;
  else if (value == "CenterFill") *mode = image_layout::YMode::CenterFill;
  else if (value == "BottomFill") *mode = image_layout::YMode::BottomFill;
  else if (value == "Top") *mode = image_layout::YMode::Top;
  else if (value == "Center") *mode = image_layout::YMode::Center;
  else if (value == "Bottom") *mode = image_layout::YMode::Bottom;
  else return false;
  return true;
}
EC_BlockPar::TBlockParEC* StyleByName(EC_BlockPar::TBlockParEC* styles, const std::string& name) {
  if (!styles) return nullptr;
  for (std::int32_t index{}; index < styles->GetBlockCount(); ++index)
    if (Text(styles->GetBlockNameByIndex(index)) == name) return styles->GetBlockByIndex(index);
  return nullptr;
}
bool ApplyOne(ui::UiObject* object, EC_BlockPar::TBlockParEC* block, const Context& context, std::string* error) {
  if (Has(block, u"Pos")) {
    const auto parts = Split(Text(block->GetParam(u"Pos"sv))); std::int32_t x{}, y{};
    if (parts.size() < 2 || !Number(parts[0], &x) || !Number(parts[1], &y)) return Fail(error, "invalid UI Pos");
    object->SetPosition({x, y});
    if (parts.size() >= 3 && !parts[2].empty()) { double depth{}; if (!context.resolve_depth || !context.resolve_depth(parts[2], &depth)) return Fail(error, "unresolved UI Pos depth"); object->SetDepth(depth); }
    if (parts.size() >= 4) object->SetPositionModeW(parts[3] == "w");
  }
  if (Has(block, u"PosZ")) { double depth{}; const auto name = Text(block->GetParam(u"PosZ"sv)); if (!context.resolve_depth || !context.resolve_depth(name, &depth)) return Fail(error, "unresolved UI PosZ"); object->SetDepth(depth); }
  if (Has(block, u"Size")) { const auto parts = Split(Text(block->GetParam(u"Size"sv))); std::int32_t width{}, height{}; if (parts.size() != 2 || !Number(parts[0], &width) || !Number(parts[1], &height)) return Fail(error, "invalid UI Size"); object->SetSize({width, height}); }
  if (Has(block, u"Sme")) { const auto parts = Split(Text(block->GetParam(u"Sme"sv))); std::int32_t x{}, y{}; if (parts.size() != 2 || !Number(parts[0], &x) || !Number(parts[1], &y)) return Fail(error, "invalid UI Sme"); object->SetOrigin({x, y}); }
  if (Has(block, u"Name")) object->SetName(Text(block->GetParam(u"Name"sv)));
  if (Has(block, u"Active")) object->SetActive(Text(block->GetParam(u"Active"sv)) != "False");
  return true;
}
bool ApplyRecursive(ui::UiObject* object, EC_BlockPar::TBlockParEC* block, const Context& context,
                    std::unordered_set<const EC_BlockPar::TBlockParEC*>* active, unsigned depth, std::string* error) {
  if (!object || !block) return Fail(error, "UI config object or block is null");
  if (depth >= 32 || !active->insert(block).second) return Fail(error, "UI Style recursion");
  if (Has(block, u"Style")) {
    auto* style = StyleByName(context.styles, Text(block->GetParam(u"Style"sv)));
    if (!style || !ApplyRecursive(object, style, context, active, depth + 1, error)) return false;
  }
  active->erase(block);
  return ApplyOne(object, block, context, error);
}
bool VisitStyleChain(EC_BlockPar::TBlockParEC* block, const Context& context,
                     std::unordered_set<const EC_BlockPar::TBlockParEC*>* active,
                     unsigned depth, const std::function<bool(EC_BlockPar::TBlockParEC*)>& visit,
                     std::string* error) {
  if (!block || depth >= 32 || !active->insert(block).second) return Fail(error, "UI Style recursion");
  if (Has(block, u"Style")) {
    auto* style = StyleByName(context.styles, Text(block->GetParam(u"Style"sv)));
    if (!style || !VisitStyleChain(style, context, active, depth + 1, visit, error)) return false;
  }
  active->erase(block);
  return visit(block);
}
bool ApplyImageProperties(ui::UiImageLeaf* leaf, EC_BlockPar::TBlockParEC* block, const Context& context,
                          image_object::Kind kind, bool load_resource, std::string* error) {
  std::string resource, option;
  image_layout::XMode x_mode = image_layout::XMode::Center;
  image_layout::YMode y_mode = image_layout::YMode::Center;
  bool half_alpha{};
  std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
  const auto visit = [&](EC_BlockPar::TBlockParEC* source) {
    if (Has(source, u"Image")) resource = Text(source->GetParam(u"Image"sv));
    if (Has(source, u"KindX") && !XMode(Text(source->GetParam(u"KindX"sv)), &x_mode)) return Fail(error, "invalid UI KindX");
    if (Has(source, u"AlignX") && !XMode(Text(source->GetParam(u"AlignX"sv)), &x_mode)) return Fail(error, "invalid UI AlignX");
    if (Has(source, u"KindY") && !YMode(Text(source->GetParam(u"KindY"sv)), &y_mode)) return Fail(error, "invalid UI KindY");
    if (Has(source, u"AlignY") && !YMode(Text(source->GetParam(u"AlignY"sv)), &y_mode)) return Fail(error, "invalid UI AlignY");
    if (Has(source, u"HalfAlpha")) half_alpha = Enabled(Text(source->GetParam(u"HalfAlpha"sv)));
    return true;
  };
  if (!VisitStyleChain(block, context, &active, 0, visit, error)) return false;
  leaf->Image().SetModes(x_mode, y_mode);
  leaf->Image().SetHalfAlpha(half_alpha);
  if (!load_resource || resource.empty()) return true;
  if (!context.resources) return Fail(error, "UI image resource resolver is null");
  return context.resources->LoadImage(leaf, kind, resource, option, error);
}
bool ApplyZoneProperties(ui::UiZone* zone, EC_BlockPar::TBlockParEC* block,
                         const Context& context, std::string* error) {
  std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
  return VisitStyleChain(block, context, &active, 0, [&](EC_BlockPar::TBlockParEC* source) {
    if (!Has(source, u"Kind")) return true;
    const auto kind = Text(source->GetParam(u"Kind"sv));
    if (kind == "Rect") zone->SetZoneKind(ui::ZoneKind::Rect);
    else if (kind == "Circle") zone->SetZoneKind(ui::ZoneKind::Circle);
    else return Fail(error, "invalid Zone Kind");
    return true;
  }, error);
}
bool IsEventBlock(const std::string& name) {
  return name == "OnPressCode" || name == "OnMouseEnterCode" || name == "OnMouseLeaveCode" ||
         name == "OnMouseRightClick" || name == "OnKey";
}
bool IsKnownControl(const std::string& name) {
  static const char* const names[] = {"Panel", "PanelScrollBar", "Window", "SimpleImage", "TransImage", "AlphaImage", "RotateImage", "RotateImage2", "RotateImage5", "RotateImageGAI", "Image", "InfiniteImage", "AImage", "GI", "GAI", "GAIFile", "MultiImage", "Door", "SimpleButton", "TextButton", "GraphButton", "Zone", "Label", "Edit", "ScrollBar", "CountBar", "SBPath", "StatusBar", "Planet", "PlanetButton", "CheckBox", "RadioGroup", "Grid", "Line", "Circle", "Frame", "ShrLight", "GraphBuf", "StarField", "StarFieldM", "StarFieldImg", "SpaceCircle", "SpaceImg", "PolyLine", "XviD"};
  for (const char* candidate : names) if (name == candidate) return true;
  return false;
}
bool ParseGenericImage(const std::string& image, image_object::Kind* kind, std::string* resource, std::string* error) {
  const auto parts = Split(image);
  if (parts.empty() || parts[0].empty()) return Fail(error, "generic Image is empty");
  if (parts.size() == 1) { *kind = image_object::Kind::Simple; *resource = parts[0]; return true; }
  if (parts[0] == "Simple") *kind = image_object::Kind::Simple;
  else if (parts[0] == "Trans") *kind = image_object::Kind::Trans;
  else if (parts[0] == "Alpha") *kind = image_object::Kind::Alpha;
  else return Fail(error, "unsupported generic Image mode");
  *resource = parts[1];
  return resource->empty() ? Fail(error, "generic Image resource is empty") : true;
}
std::u16string Wide(const pas::WideString& value) {
  return {value.pchar(), static_cast<std::size_t>(value.length())};
}
std::u16string Trim(std::u16string value) {
  const auto first = value.find_first_not_of(u" \t\r\n");
  if (first == std::u16string::npos) return {};
  const auto last = value.find_last_not_of(u" \t\r\n");
  return value.substr(first, last - first + 1);
}
bool ParseColor(const std::string& text, std::uint16_t* color) {
  const auto parts = Split(text); std::int32_t red{}, green{}, blue{};
  if (parts.size() != 3 || !Number(parts[0], &red) || !Number(parts[1], &green) ||
      !Number(parts[2], &blue) || red < 0 || green < 0 || blue < 0) return false;
  *color = tagged_text::PackRgb565(red, green, blue); return true;
}
bool LabelX(const std::string& value, ui::LabelAlignX* out) {
  if (value == "Left") *out = ui::LabelAlignX::Left;
  else if (value == "Center") *out = ui::LabelAlignX::Center;
  else if (value == "Right") *out = ui::LabelAlignX::Right;
  else if (value == "Auto") *out = ui::LabelAlignX::Auto;
  else return false;
  return true;
}
bool LabelY(const std::string& value, ui::LabelAlignY* out) {
  if (value == "Top") *out = ui::LabelAlignY::Top;
  else if (value == "Center") *out = ui::LabelAlignY::Center;
  else if (value == "CenterEx") *out = ui::LabelAlignY::CenterEx;
  else if (value == "Bottom") *out = ui::LabelAlignY::Bottom;
  else if (value == "Auto") *out = ui::LabelAlignY::Auto;
  else return false;
  return true;
}
bool ApplyLabelProperties(ui::UiLabelLeaf* label, EC_BlockPar::TBlockParEC* block,
                          const Context& context, std::string* error) {
  std::string font_key, image;
  std::vector<std::u16string> lines;
  std::uint16_t text_color = 0xffff, border_light = 0xffff,
                border_dark = tagged_text::PackRgb565(55, 55, 55),
                text_border_color = 0, text_shadow_color = 0;
  std::int32_t text_border = 0, text_shadow = 0;
  bool border = false, word_wrap = false;
  ui::LabelAlignX align_x = ui::LabelAlignX::Center;
  ui::LabelAlignY align_y = ui::LabelAlignY::Center;
  image_layout::XMode image_x = image_layout::XMode::Center;
  image_layout::YMode image_y = image_layout::YMode::Center;
  std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
  const auto visit = [&](EC_BlockPar::TBlockParEC* source) {
    if (Has(source, u"Font")) font_key = Text(source->GetParam(u"Font"sv));
    if (Has(source, u"Text")) {
      lines.clear();
      const auto count = source->CountParams(u"Text");
      for (std::int32_t i = 0; i < count; ++i) {
        const auto path = pas::concat_wide({u"Text"_wref.get(), u":"_wref.get(), pas::wide_int_to_str(i)});
        lines.push_back(Wide(source->GetParamByPath(path)));
      }
    }
    if (Has(source, u"Image")) image = Text(source->GetParam(u"Image"sv));
    if (Has(source, u"ImageKindX") && !XMode(Text(source->GetParam(u"ImageKindX"sv)), &image_x)) return Fail(error, "invalid Label ImageKindX");
    if (Has(source, u"ImageKindY") && !YMode(Text(source->GetParam(u"ImageKindY"sv)), &image_y)) return Fail(error, "invalid Label ImageKindY");
    if (Has(source, u"TextColor") && !ParseColor(Text(source->GetParam(u"TextColor"sv)), &text_color)) return Fail(error, "invalid Label TextColor");
    if (Has(source, u"Border")) border = Text(source->GetParam(u"Border"sv)) == "True";
    if (Has(source, u"BorderLightColor")) {
      if (!ParseColor(Text(source->GetParam(u"BorderLightColor"sv)), &border_light)) return Fail(error, "invalid Label BorderLightColor");
      border_dark = border_light;
    }
    if (Has(source, u"BorderDarkColor") && !ParseColor(Text(source->GetParam(u"BorderDarkColor"sv)), &border_dark)) return Fail(error, "invalid Label BorderDarkColor");
    if (Has(source, u"WordWrap")) word_wrap = Enabled(Text(source->GetParam(u"WordWrap"sv)));
    if (Has(source, u"AlignX") && !LabelX(Text(source->GetParam(u"AlignX"sv)), &align_x)) return Fail(error, "invalid Label AlignX");
    if (Has(source, u"AlignY") && !LabelY(Text(source->GetParam(u"AlignY"sv)), &align_y)) return Fail(error, "invalid Label AlignY");
    if (Has(source, u"TextBorder") && (!Number(Text(source->GetParam(u"TextBorder"sv)), &text_border) || text_border < 0)) return Fail(error, "invalid Label TextBorder");
    if (Has(source, u"TextBorderColor") && !ParseColor(Text(source->GetParam(u"TextBorderColor"sv)), &text_border_color)) return Fail(error, "invalid Label TextBorderColor");
    if (Has(source, u"TextShadow") && (!Number(Text(source->GetParam(u"TextShadow"sv)), &text_shadow) || text_shadow < 0)) return Fail(error, "invalid Label TextShadow");
    if (Has(source, u"TextShadowColor") && !ParseColor(Text(source->GetParam(u"TextShadowColor"sv)), &text_shadow_color)) return Fail(error, "invalid Label TextShadowColor");
    return true;
  };
  if (!VisitStyleChain(block, context, &active, 0, visit, error)) return false;
  if (!lines.empty() && context.language) {
    std::u16string key;
    for (std::size_t i = 0; i < lines.size(); ++i) { if (i) key += u"\n"; key += lines[i]; }
    key = Trim(std::move(key));
    const auto count = context.language->CountParamsByPath(pas::WideString(key.c_str()));
    if (count > 0) {
      lines.clear();
      for (std::int32_t i = 0; i < count; ++i) {
        auto path = key + u":";
        for (char digit : std::to_string(i)) path.push_back(static_cast<char16_t>(digit));
        lines.push_back(Wide(context.language->GetParamByPathOrMarker(pas::WideString(path.c_str()))));
      }
    }
  }
  if (context.resolve_label_font_alias) font_key = context.resolve_label_font_alias(font_key);
  if (!font_key.empty()) {
    if (!context.fonts) return Fail(error, "Label font repository is null");
    std::shared_ptr<const aft_font::AftFont> font;
    if (!context.fonts->Acquire(font_key, &font, nullptr, error)) return false;
    label->SetFont(font_key, std::move(font));
  }
  label->SetTextLines(std::move(lines)); label->SetTextColor(text_color);
  label->SetBorder(border, border_light, border_dark);
  label->SetWordWrap(word_wrap); label->SetAlignX(align_x); label->SetAlignY(align_y);
  label->SetTextBorder(text_border, text_border_color);
  label->SetTextShadow(text_shadow, text_shadow_color);
  if (!image.empty()) {
    image_object::Kind kind{}; std::string resource;
    if (!ParseGenericImage(image, &kind, &resource, error)) return false;
    if (!context.resources) return Fail(error, "Label image resolver is null");
    ui::UiImageLeaf temporary;
    temporary.Image().SetModes(image_x, image_y);
    if (!context.resources->LoadImage(&temporary, kind, resource, "", error)) return false;
    label->SetBackground(std::move(temporary.Image()));
  }
  return label->Prepare(error);
}
bool ApplyGraphButtonProperties(ui::UiGraphButton* button, EC_BlockPar::TBlockParEC* block,
                                const Context& context, std::string* error) {
  constexpr const char16_t* slots[] = {u"ImageNormal", u"ImageNormalA", u"ImageDown",
      u"ImageDownA", u"ImageDisable", u"ImageDisableA", u"ImageHit"};
  constexpr const char16_t* colors[] = {u"CaptionColorNormal", u"CaptionColorNormalA",
      u"CaptionColorDown", u"CaptionColorDownA", u"CaptionColorDisable", u"CaptionColorDisableA"};
  constexpr const char16_t* shadows[] = {u"CaptionShadowColorNormal", u"CaptionShadowColorNormalA",
      u"CaptionShadowColorDown", u"CaptionShadowColorDownA", u"CaptionShadowColorDisable", u"CaptionShadowColorDisableA"};
  std::array<std::string, 7> images;
  std::array<ui::Point, 7> offsets{};
  std::array<bool, 7> has_offset{};
  std::array<std::uint16_t, 6> caption_colors{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
  std::array<std::uint16_t, 6> caption_shadows{};
  std::string font_key, sound_enter, sound_leave, sound_click, auto_flags;
  std::u16string caption;
  ui::LabelAlignX align_x = ui::LabelAlignX::Center;
  ui::LabelAlignY align_y = ui::LabelAlignY::CenterEx;
  ui::Point caption_normal{}, caption_down{};
  std::int32_t shadow_offset{};
  std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
  const auto visit = [&](EC_BlockPar::TBlockParEC* source) {
    if (Has(source, u"Kind")) {
      const auto value = Text(source->GetParam(u"Kind"sv));
      if (value == "Normal") button->SetButtonKind(ui::GraphButtonKind::Normal);
      else if (value == "Fix") button->SetButtonKind(ui::GraphButtonKind::Fix);
      else if (value == "Disable") button->SetButtonKind(ui::GraphButtonKind::Disable);
      else if (value == "FixDisable") button->SetButtonKind(ui::GraphButtonKind::FixDisable);
      else return Fail(error, "invalid GraphButton Kind");
    }
    if (Has(source, u"KindHit")) {
      const auto value = Text(source->GetParam(u"KindHit"sv));
      if (value == "Rect") button->SetHitKind(ui::GraphButtonHitKind::Rect);
      else if (value == "Graph") button->SetHitKind(ui::GraphButtonHitKind::Graph);
      else if (value == "ImageHit") button->SetHitKind(ui::GraphButtonHitKind::ImageHit);
      else return Fail(error, "invalid GraphButton KindHit");
    }
    if (Has(source, u"Font")) font_key = Text(source->GetParam(u"Font"sv));
    if (Has(source, u"Caption")) caption = Wide(source->GetParam(u"Caption"sv));
    if (Has(source, u"CaptionColor")) {
      std::uint16_t color{};
      if (!ParseColor(Text(source->GetParam(u"CaptionColor"sv)), &color)) return Fail(error, "invalid GraphButton CaptionColor");
      caption_colors.fill(color);
    }
    if (Has(source, u"CaptionShadow") &&
        (!Number(Text(source->GetParam(u"CaptionShadow"sv)), &shadow_offset) || shadow_offset < 0))
      return Fail(error, "invalid GraphButton CaptionShadow");
    for (std::size_t i{}; i < 6; ++i) {
      if (Has(source, colors[i]) && !ParseColor(Text(source->GetParam(std::u16string_view(colors[i]))), &caption_colors[i]))
        return Fail(error, "invalid GraphButton state caption color");
      if (Has(source, shadows[i]) && !ParseColor(Text(source->GetParam(std::u16string_view(shadows[i]))), &caption_shadows[i]))
        return Fail(error, "invalid GraphButton state caption shadow color");
    }
    if (Has(source, u"CaptionAlignX") && !LabelX(Text(source->GetParam(u"CaptionAlignX"sv)), &align_x))
      return Fail(error, "invalid GraphButton CaptionAlignX");
    if (Has(source, u"CaptionAlignY") && !LabelY(Text(source->GetParam(u"CaptionAlignY"sv)), &align_y))
      return Fail(error, "invalid GraphButton CaptionAlignY");
    if (Has(source, u"CaptionSme")) {
      const auto values = Split(Text(source->GetParam(u"CaptionSme"sv)));
      if (values.size() != 4 || !Number(values[0], &caption_normal.x) ||
          !Number(values[1], &caption_normal.y) || !Number(values[2], &caption_down.x) ||
          !Number(values[3], &caption_down.y)) return Fail(error, "invalid GraphButton CaptionSme");
    }
    for (std::size_t i{}; i < images.size(); ++i) {
      if (Has(source, slots[i])) images[i] = Text(source->GetParam(std::u16string_view(slots[i])));
      const auto offset_name = std::u16string(slots[i]) + u"_Pos";
      if (Has(source, offset_name.c_str())) {
        const auto values = Split(Text(source->GetParam(std::u16string_view(offset_name))));
        if (values.size() != 2 || !Number(values[0], &offsets[i].x) ||
            !Number(values[1], &offsets[i].y)) return Fail(error, "invalid GraphButton image offset");
        has_offset[i] = true;
      }
    }
    if (Has(source, u"Disable")) button->SetDisabled(Enabled(Text(source->GetParam(u"Disable"sv))));
    if (Has(source, u"Down")) button->SetDown(Enabled(Text(source->GetParam(u"Down"sv))));
    if (Has(source, u"UpOnlyDown")) button->SetUpOnlyDown(Enabled(Text(source->GetParam(u"UpOnlyDown"sv))));
    if (Has(source, u"Auto")) auto_flags = Text(source->GetParam(u"Auto"sv));
    if (Has(source, u"SoundEnter")) sound_enter = Text(source->GetParam(u"SoundEnter"sv));
    if (Has(source, u"SoundLeave")) sound_leave = Text(source->GetParam(u"SoundLeave"sv));
    if (Has(source, u"SoundClick")) sound_click = Text(source->GetParam(u"SoundClick"sv));
    if (source->CountBlocks(u"OnPressCode"_wref.get()) > 0) button->SetHasOnPressCode(true);
    return true;
  };
  if (!VisitStyleChain(block, context, &active, 0, visit, error)) return false;
  button->SetCaptionColors(caption_colors);
  button->SetCaptionShadowColors(caption_shadows);
  button->SetCaptionShadowOffset(shadow_offset);
  button->SetCaptionOffsets(caption_normal, caption_down);
  button->SetSoundMetadata(sound_enter, sound_leave, sound_click);
  for (std::size_t i{}; i < images.size(); ++i) {
    if (images[i].empty()) continue;
    image_object::Kind kind{}; std::string resource;
    if (!ParseGenericImage(images[i], &kind, &resource, error)) return false;
    if (!context.resources) return Fail(error, "GraphButton image resolver is null");
    auto image = std::make_unique<ui::UiImageLeaf>();
    if (!context.resources->LoadImage(image.get(), kind, resource, "", error)) return false;
    const auto slot = static_cast<ui::GraphButtonSlot>(i);
    if (!button->AddStateImage(slot, std::move(image), error)) return false;
    if (has_offset[i]) button->SetStateOffset(slot,
        {offsets[i].x - button->LocalPosition().x, offsets[i].y - button->LocalPosition().y});
  }
  if (!caption.empty() && context.language && context.language->CountParamsByPath(pas::WideString(caption.c_str())) > 0)
    caption = Wide(context.language->GetParamByPathOrMarker(pas::WideString(caption.c_str())));
  if (!font_key.empty() || !caption.empty()) {
    if (context.resolve_label_font_alias) font_key = context.resolve_label_font_alias(font_key);
    if (!context.fonts) return Fail(error, "GraphButton font repository is null");
    std::shared_ptr<const aft_font::AftFont> font;
    if (!context.fonts->Acquire(font_key, &font, nullptr, error)) return false;
    auto label = std::make_unique<ui::UiLabelLeaf>();
    label->SetFont(font_key, std::move(font));
    label->SetTextLines({caption});
    label->SetAlignX(align_x); label->SetAlignY(align_y);
    if (!button->AddCaption(std::move(label), error)) return false;
    if (!button->Caption()->Prepare(error)) return false;
  }
  bool auto_position{}, auto_size{};
  if (!auto_flags.empty()) {
    for (const auto& flag : Split(auto_flags)) {
      if (flag == "Pos") auto_position = true;
      else if (flag == "Size") auto_size = true;
      else return Fail(error, "invalid GraphButton Auto");
    }
    button->UpdateAutoGeometry(auto_position, auto_size);
  }
  return true;
}
bool ApplyWindowProperties(ui::UiWindow* window, EC_BlockPar::TBlockParEC* block,
                           const Context& context, std::string* error) {
  constexpr const char16_t* slots[] = {u"ImageTopLeft", u"ImageTopRight",
      u"ImageBottomLeft", u"ImageBottomRight", u"ImageLeft", u"ImageRight",
      u"ImageTop", u"ImageBottom", u"ImageTexture"};
  std::array<std::string, 9> images;
  std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
  const auto visit = [&](EC_BlockPar::TBlockParEC* source) {
    for (std::size_t i{}; i < images.size(); ++i)
      if (Has(source, slots[i])) images[i] = Text(source->GetParam(std::u16string_view(slots[i])));
    if (Has(source, u"MinSize")) {
      const auto values = Split(Text(source->GetParam(u"MinSize"sv)));
      ui::Size minimum{};
      if (values.size() != 2 || !Number(values[0], &minimum.width) ||
          !Number(values[1], &minimum.height) || minimum.width < 0 || minimum.height < 0)
        return Fail(error, "invalid Window MinSize");
      window->SetMinimumSize(minimum);
    }
    if (Has(source, u"WorkSubRect")) {
      const auto values = Split(Text(source->GetParam(u"WorkSubRect"sv)));
      ui::Rect rect{};
      if (values.size() != 4 || !Number(values[0], &rect.left) ||
          !Number(values[1], &rect.top) || !Number(values[2], &rect.right) ||
          !Number(values[3], &rect.bottom)) return Fail(error, "invalid Window WorkSubRect");
      window->SetWorkSubRect(rect);
    }
    return true;
  };
  if (!VisitStyleChain(block, context, &active, 0, visit, error)) return false;
  std::array<image_object::Kind, 9> image_kinds{};
  std::array<std::string, 9> resources;
  for (std::size_t i{}; i < images.size(); ++i)
    if (!images[i].empty() &&
        !ParseGenericImage(images[i], &image_kinds[i], &resources[i], error)) return false;
  constexpr ui::WindowSlot upstream_order[] = {ui::WindowSlot::Left, ui::WindowSlot::Right,
      ui::WindowSlot::Top, ui::WindowSlot::Bottom, ui::WindowSlot::TopLeft,
      ui::WindowSlot::TopRight, ui::WindowSlot::BottomLeft,
      ui::WindowSlot::BottomRight, ui::WindowSlot::Texture};
  for (const auto slot : upstream_order) {
    const auto i = static_cast<std::size_t>(slot);
    if (images[i].empty()) return Fail(error, "Window border image is missing");
    if (!context.resources) return Fail(error, "Window image resolver is null");
    auto image = std::make_unique<ui::UiImageLeaf>();
    if (!context.resources->LoadImage(image.get(), image_kinds[i], resources[i], "", error)) return false;
    if (!window->AddBorderImage(slot, std::move(image), error)) return false;
  }
  return window->FinalizeLayout(error);
}
bool LoadOne(ui::UiObject* parent, const std::string& name, EC_BlockPar::TBlockParEC* block,
             const Context& context, LoadMode mode, LoadReport* report, std::string* error) {
  if (IsEventBlock(name)) { if (report) report->skipped_events.push_back(name); return true; }
  if (!IsKnownControl(name)) return LoadChildren(parent, block, context, mode, report, error);
  std::unique_ptr<ui::UiObject> node;
  image_object::Kind image_kind = image_object::Kind::Simple;
  if (name == "Panel") node = std::make_unique<ui::UiPanel>();
  else if (name == "Window") node = std::make_unique<ui::UiWindow>();
  else if (name == "GraphButton") node = std::make_unique<ui::UiGraphButton>();
  else if (name == "Zone") node = std::make_unique<ui::UiZone>();
  else if (name == "Label") node = std::make_unique<ui::UiLabelLeaf>();
  else if (name == "SimpleImage") { node = std::make_unique<ui::UiImageLeaf>(); image_kind = image_object::Kind::Simple; }
  else if (name == "TransImage") { node = std::make_unique<ui::UiImageLeaf>(); image_kind = image_object::Kind::Trans; }
  else if (name == "AlphaImage") { node = std::make_unique<ui::UiImageLeaf>(); image_kind = image_object::Kind::Alpha; }
  else if (name == "Image") {
    std::string raw;
    std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
    if (!VisitStyleChain(block, context, &active, 0, [&](EC_BlockPar::TBlockParEC* source) { if (Has(source, u"Image")) raw = Text(source->GetParam(u"Image"sv)); return true; }, error)) return false;
    std::string resource;
    if (!ParseGenericImage(raw, &image_kind, &resource, error)) return false;
    node = std::make_unique<ui::UiImageLeaf>();
  } else {
    if (report) report->unsupported_controls.push_back(name);
    if (mode == LoadMode::Inventory) return true;
    if (error) *error = "unsupported UI control: " + name;
    return false;
  }
  if (!ApplyBaseProperties(node.get(), block, context, error)) return false;
  if (auto* image = dynamic_cast<ui::UiImageLeaf*>(node.get())) {
    if (name == "Image") {
      std::string raw, resource;
      std::unordered_set<const EC_BlockPar::TBlockParEC*> active;
      if (!VisitStyleChain(block, context, &active, 0, [&](EC_BlockPar::TBlockParEC* source) { if (Has(source, u"Image")) raw = Text(source->GetParam(u"Image"sv)); return true; }, error) || !ParseGenericImage(raw, &image_kind, &resource, error)) return false;
      if (!ApplyImageProperties(image, block, context, image_kind, false, error)) return false;
      if (!context.resources) return Fail(error, "UI image resource resolver is null");
      if (!context.resources->LoadImage(image, image_kind, resource, "", error)) return false;
    } else if (!ApplyImageProperties(image, block, context, image_kind, true, error)) return false;
  }
  if (auto* label = dynamic_cast<ui::UiLabelLeaf*>(node.get()))
    if (!ApplyLabelProperties(label, block, context, error)) return false;
  bool control_ready = true;
  if (auto* zone = dynamic_cast<ui::UiZone*>(node.get()))
    control_ready = ApplyZoneProperties(zone, block, context, error);
  if (auto* button = dynamic_cast<ui::UiGraphButton*>(node.get()))
    control_ready = ApplyGraphButtonProperties(button, block, context, error);
  if (auto* window = dynamic_cast<ui::UiWindow*>(node.get()))
    control_ready = ApplyWindowProperties(window, block, context, error);
  if (!control_ready) {
    if (mode == LoadMode::Strict) return false;
    if (report) report->unsupported_controls.push_back(name);
    if (error) error->clear();
    return true;
  }
  auto* attached = node.get();
  if (!parent->Attach(std::move(node), error)) return false;
  if ((attached->Kind() == ui::NodeKind::Panel || attached->Kind() == ui::NodeKind::Zone ||
       attached->Kind() == ui::NodeKind::GraphButton || attached->Kind() == ui::NodeKind::Window) &&
      !LoadChildren(attached, block, context, mode, report, error)) return false;
  return true;
}
}  // namespace

bool ApplyBaseProperties(ui::UiObject* object, EC_BlockPar::TBlockParEC* block, const Context& context, std::string* error) {
  std::unordered_set<const EC_BlockPar::TBlockParEC*> active; return ApplyRecursive(object, block, context, &active, 0, error);
}
bool ResolveRuntimeDepth(EC_BlockPar::TBlockParEC* depth_config, const std::string& name, double* value) {
  if (!value || name.empty()) return false;
  if (depth_config) for (std::int32_t index{}; index < depth_config->GetParamCount(); ++index) {
    if (Text(depth_config->GetParamName(index)) == name) return Decimal(Text(depth_config->GetParamValue(index)), value);
  }
  return Decimal(name, value);
}

bool PackageUiResourceResolver::LoadImage(ui::UiImageLeaf* leaf, image_object::Kind kind,
                                          const std::string& resource, const std::string& option,
                                          std::string* error) {
  if (!leaf || !package_) return Fail(error, "UI package resource resolver is null");
  leaf->Image().SetPackage(package_);
  return leaf->Load(kind, resource, option, error);
}
bool LoadChildren(ui::UiObject* parent, EC_BlockPar::TBlockParEC* block, const Context& context,
                  LoadMode mode, LoadReport* report, std::string* error) {
  if (!parent || !block) return Fail(error, "UI config parent or block is null");
  for (std::int32_t index{}; index < block->GetBlockCount(); ++index) {
    if (!LoadOne(parent, Text(block->GetBlockNameByIndex(index)), block->GetBlockByIndex(index), context, mode, report, error)) return false;
  }
  return true;
}
bool LoadLabel(ui::UiObject* parent, EC_BlockPar::TBlockParEC* block,
               const Context& context, std::string* error) {
  return LoadOne(parent, "Label", block, context, LoadMode::Strict, nullptr, error);
}

}  // namespace srhd_awa::platform::ui_config

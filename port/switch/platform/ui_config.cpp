#include "ui_config.hpp"

#include "image_layout.hpp"
#include "image_object.hpp"
#include "package.hpp"
#include "ui_tree_renderer.hpp"
#include "units/EC_BlockPar.hpp"

#include <charconv>
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
bool LoadOne(ui::UiObject* parent, const std::string& name, EC_BlockPar::TBlockParEC* block,
             const Context& context, LoadMode mode, LoadReport* report, std::string* error) {
  if (IsEventBlock(name)) { if (report) report->skipped_events.push_back(name); return true; }
  if (!IsKnownControl(name)) return LoadChildren(parent, block, context, mode, report, error);
  std::unique_ptr<ui::UiObject> node;
  image_object::Kind image_kind = image_object::Kind::Simple;
  if (name == "Panel") node = std::make_unique<ui::UiPanel>();
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
  auto* attached = node.get();
  if (!parent->Attach(std::move(node), error)) return false;
  if (attached->Kind() == ui::NodeKind::Panel && !LoadChildren(attached, block, context, mode, report, error)) return false;
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

}  // namespace srhd_awa::platform::ui_config

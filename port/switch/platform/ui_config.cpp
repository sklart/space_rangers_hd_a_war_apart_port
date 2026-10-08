#include "ui_config.hpp"

#include "units/EC_BlockPar.hpp"

#include <charconv>
#include <cstdlib>
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

}  // namespace srhd_awa::platform::ui_config

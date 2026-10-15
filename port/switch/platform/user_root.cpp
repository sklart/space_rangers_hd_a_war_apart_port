#include "user_root.hpp"

#include <filesystem>
#include <string_view>

namespace srhd_awa::platform::user_root {
namespace {
std::filesystem::path g_root;

bool IsContained(const std::filesystem::path& child, const std::filesystem::path& parent) {
  const auto normalized_child = child.lexically_normal();
  const auto normalized_parent = parent.lexically_normal();
  auto child_part = normalized_child.begin();
  for (auto parent_part = normalized_parent.begin(); parent_part != normalized_parent.end(); ++parent_part, ++child_part) {
    if (child_part == normalized_child.end() || *child_part != *parent_part) return false;
  }
  return true;
}

bool SameAsciiName(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto fold = [](unsigned char value) {
      return value >= 'A' && value <= 'Z' ? value - 'A' + 'a' : value;
    };
    if (fold(static_cast<unsigned char>(a[i])) !=
        fold(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}
}  // namespace

void SetRoot(const std::string& root) { g_root = std::filesystem::path(root).lexically_normal(); }

std::string ConfigDirectory() {
  if (g_root.empty()) return {};
  auto result = (g_root / "config").generic_string();
  if (result.empty() || result.back() != '/') result.push_back('/');
  return result;
}

bool EnsureLayout() {
  if (g_root.empty()) return false;
  std::error_code error;
  std::filesystem::create_directories(g_root / "config", error);
  if (error) return false;
  std::filesystem::create_directories(g_root / "save", error);
  if (error) return false;
  std::filesystem::create_directories(g_root / "logs", error);
  if (error) return false;
  std::filesystem::create_directories(g_root / "runtime", error);
  return !error;
}

std::string ResolveConfigPath(const std::string& path) {
  if (g_root.empty() || path.empty()) return {};
  std::string normalized = path;
  for (char& value : normalized) if (value == '\\') value = '/';
  const std::filesystem::path input(normalized);
  if (!input.is_absolute() && input.has_root_name()) return {};
  const std::filesystem::path config = (g_root / "config").lexically_normal();
  // GetGameUserDirectory() already supplies a config-root-relative path.  Do
  // not append config/ again when that path reaches CreateNew/SaveTextFile.
  const std::filesystem::path resolved =
      (input.is_absolute() || IsContained(input, config) ? input : config / input).lexically_normal();
  if (!IsContained(resolved, config)) return {};
  std::error_code error;
  if (std::filesystem::exists(resolved, error)) return resolved.generic_string();
  if (error) return {};
  const auto parent = resolved.parent_path();
  if (!std::filesystem::is_directory(parent, error))
    return resolved.generic_string();
  std::string matching_path;
  for (const auto& entry : std::filesystem::directory_iterator(parent, error)) {
    if (error) break;
    if (!SameAsciiName(entry.path().filename().string(), resolved.filename().string()))
      continue;
    if (!matching_path.empty()) return {};  // Ambiguous spelling on case-sensitive hosts.
    matching_path = entry.path().generic_string();
  }
  if (error) return {};
  return matching_path.empty() ? resolved.generic_string() : matching_path;
}

}  // namespace srhd_awa::platform::user_root

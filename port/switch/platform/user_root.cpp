#include "user_root.hpp"

#include <filesystem>

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
  const std::filesystem::path resolved = (input.is_absolute() ? input : config / input).lexically_normal();
  return IsContained(resolved, config) ? resolved.generic_string() : std::string{};
}

}  // namespace srhd_awa::platform::user_root

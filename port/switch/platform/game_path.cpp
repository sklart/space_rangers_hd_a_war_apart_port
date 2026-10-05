#include "game_path.hpp"

#include <algorithm>
#include <filesystem>

namespace srhd_awa::platform::game_path {
namespace {
std::string g_root;

std::string NormalizeRelative(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  const std::filesystem::path input(path);
  if (input.is_absolute() || input.has_root_name()) return {};
  std::filesystem::path result;
  for (const auto& component : input) {
    if (component == "." || component.empty()) continue;
    const auto name = component.generic_string();
    if (component == ".." || component.is_absolute() || name.find(':') != std::string::npos) return {};
    result /= component;
  }
  return result.generic_string();
}

bool EqualsAsciiInsensitive(const std::string& left, const std::string& right) {
  if (left.size() != right.size()) return false;
  for (size_t index = 0; index < left.size(); ++index) {
    const auto fold = [](unsigned char value) {
      return value >= 'a' && value <= 'z' ? static_cast<unsigned char>(value - 'a' + 'A') : value;
    };
    if (fold(left[index]) != fold(right[index])) return false;
  }
  return true;
}
}  // namespace

void SetRoot(const std::string& root) { g_root = root; }

std::string Resolve(const std::string& game_relative_path) {
  const std::string relative = NormalizeRelative(game_relative_path);
  if (relative.empty() || g_root.empty()) return {};
  std::filesystem::path resolved(g_root);
  for (const auto& component : std::filesystem::path(relative)) {
    const auto direct = resolved / component;
    if (std::filesystem::exists(direct)) {
      resolved = direct;
      continue;
    }
    std::error_code error;
    bool matched = false;
    for (const auto& entry : std::filesystem::directory_iterator(resolved, error)) {
      if (EqualsAsciiInsensitive(entry.path().filename().string(), component.string())) {
        resolved = entry.path();
        matched = true;
        break;
      }
    }
    if (error || !matched) return direct.string();
  }
  return resolved.string();
}

bool FileExists(const std::string& game_relative_path) {
  const std::string path = Resolve(game_relative_path);
  std::error_code error;
  return !path.empty() && std::filesystem::is_regular_file(path, error) && !error;
}
}  // namespace srhd_awa::platform::game_path

#include "game_path.hpp"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace srhd_awa::platform::game_path {
namespace {
std::string g_root;
std::mutex g_resolve_mutex;
std::unordered_map<std::string, std::string> g_resolved_paths;
std::unordered_map<std::string, std::string> g_missing_prefixes;

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

void SetRoot(const std::string& root) {
  std::lock_guard lock(g_resolve_mutex);
  g_root = root;
  g_resolved_paths.clear();
  g_missing_prefixes.clear();
}

std::string Resolve(const std::string& game_relative_path) {
  std::string input = game_relative_path;
  std::replace(input.begin(), input.end(), '\\', '/');
  std::lock_guard lock(g_resolve_mutex);
  if (g_root.empty()) return {};
  std::string relative;
  if (input.starts_with("sdmc:/") || std::filesystem::path(input).is_absolute()) {
    auto root = std::filesystem::path(g_root);
    if (!g_root.starts_with("sdmc:/")) root = std::filesystem::absolute(root);
    relative = NormalizeRelative(std::filesystem::path(input).lexically_relative(root).generic_string());
    if (relative.empty()) return input;
  } else relative = NormalizeRelative(input);
  if (relative.empty()) return {};
  if (const auto cached = g_resolved_paths.find(relative); cached != g_resolved_paths.end())
    return cached->second;
  std::filesystem::path resolved(g_root);
  std::string prefix;
  for (const auto& component : std::filesystem::path(relative)) {
    if (!prefix.empty()) prefix.push_back('/');
    prefix += component.generic_string();
    if (const auto missing = g_missing_prefixes.find(prefix);
        missing != g_missing_prefixes.end())
      return g_resolved_paths.emplace(relative, missing->second).first->second;
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
    if (error) return direct.string();
    if (!matched) {
      // The game installation is read-only during a run. In particular,
      // package-backed quests miss this loose path on every lookup.
      g_missing_prefixes.emplace(prefix, direct.string());
      return g_resolved_paths.emplace(relative, direct.string()).first->second;
    }
  }
  return g_resolved_paths.emplace(relative, resolved.string()).first->second;
}

bool FileExists(const std::string& game_relative_path) {
  const std::string path = Resolve(game_relative_path);
  std::error_code error;
  return !path.empty() && std::filesystem::is_regular_file(path, error) && !error;
}
}  // namespace srhd_awa::platform::game_path

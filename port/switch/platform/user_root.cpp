#include "user_root.hpp"
#include "unicode_path.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

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

bool ContainsSave(const std::filesystem::path& directory, std::error_code& error) {
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (error) break;
    if (SameAsciiName(entry.path().extension().string(), ".sav") && entry.is_regular_file(error))
      return true;
  }
  return false;
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
  // Resolve each component, including the legacy Save directory. Resolving
  // just the basename created a second, empty config/save on Linux and made
  // the old AutoSave invisible. Never move files or use the root save folder.
  auto cursor = config;
  bool first_component = true;
  for (const auto& part : resolved.lexically_relative(config)) {
    if (part == ".") continue;
    if (first_component && SameAsciiName(part.string(), "save")) {
      // Enumerate physical names, rather than stat'ing Save and save and
      // comparing inode identity. libnx reports zero inodes; on SD both
      // spellings can open the same directory while the manifest must retain
      // its actual name for every later publish/readback operation.
      std::vector<std::filesystem::path> directories;
      for (const auto& entry : std::filesystem::directory_iterator(config, error)) {
        if (error) break;
        if (SameAsciiName(entry.path().filename().string(), "save") && entry.is_directory(error))
          directories.push_back(entry.path());
      }
      if (error) return {};
      if (directories.size() > 1) {
        std::filesystem::path populated;
        for (const auto& directory : directories) {
          if (ContainsSave(directory, error)) {
            if (!populated.empty()) return {};
            populated = directory;
          }
          if (error) return {};
        }
        if (populated.empty()) return {};
        cursor = populated;
      } else if (directories.size() == 1) cursor = directories.front();
      else cursor /= part;
      first_component = false;
      continue;
    }
    first_component = false;
    auto next = cursor / part;
    if (std::filesystem::exists(next, error)) { cursor = next; continue; }
    if (error) return {};
    if (std::filesystem::is_directory(cursor, error)) {
      std::filesystem::path match;
      for (const auto& entry : std::filesystem::directory_iterator(cursor, error)) {
        if (error) break;
        if (!SameAsciiName(entry.path().filename().string(), part.string())) continue;
        if (!match.empty()) return {};
        match = entry.path();
      }
      if (error) return {};
      if (!match.empty()) next = match;
    } else if (error && error != std::errc::no_such_file_or_directory) return {};
    error.clear();
    cursor = next;
  }
  return cursor.generic_string();
}

std::string ResolveConfigPath(const char16_t* path) {
  return path ? ResolveConfigPath(PathUtf8(path)) : std::string{};
}

std::string ResolveAbsoluteConfigPath(const char16_t* path) {
  if (!path || g_root.empty()) return {};
  auto utf8 = PathUtf8(path);
  for (char& c : utf8) if (c == '\\') c = '/';
  if (!IsContained(std::filesystem::path(utf8), g_root / "config")) return {};
  return ResolveConfigPath(utf8);
}

bool ConfigFileExists(const char16_t* path) {
  const auto resolved = ResolveConfigPath(path);
  if (resolved.empty()) return false;
  std::error_code error;
  return std::filesystem::is_regular_file(std::filesystem::u8path(resolved), error) && !error;
}

}  // namespace srhd_awa::platform::user_root

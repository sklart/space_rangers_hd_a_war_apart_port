#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

namespace srhd_awa::platform::e2e5_cfg_bootstrap {
namespace fs = std::filesystem;

struct Result {
  std::string source;
  std::string target;
  std::string problem;
  std::string source_type;
  std::string target_type;
  bool source_readable{};
  bool target_readable{};
  bool target_directory_exists{};
  bool used_existing{};
  bool created{};
  std::uintmax_t space_available{};
  int error_value{};
  std::string error_message;

  bool ok() const { return used_existing || created; }
};

inline std::string TypeName(fs::file_type type) {
  switch (type) {
    case fs::file_type::regular: return "regular";
    case fs::file_type::directory: return "directory";
    case fs::file_type::not_found: return "missing";
    default: return "other";
  }
}

inline bool EqualsAsciiInsensitive(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; };
    if (fold(static_cast<unsigned char>(a[i])) != fold(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

inline bool Readable(const fs::path& path) {
  if (auto* file = std::fopen(path.string().c_str(), "rb")) {
    std::fclose(file);
    return true;
  }
  return false;
}

inline void RecordError(Result& result, const std::error_code& error) {
  result.error_value = error.value();
  result.error_message = error.message();
}

inline Result Inspect(const std::string& source, const std::string& target) {
  Result result;
  result.source = source;
  result.target = target;
  if (source.empty() || target.empty()) { result.problem = "path-resolution"; return result; }
  const fs::path requested(target);
  const fs::path directory = requested.parent_path();
  std::error_code error;
  const auto directory_status = fs::status(directory, error);
  result.target_directory_exists = !error && fs::is_directory(directory_status);
  if (!result.target_directory_exists) {
    result.problem = "target-directory-missing";
    RecordError(result, error);
    return result;
  }
  const auto space = fs::space(directory, error);
  if (!error) result.space_available = space.available;
  else RecordError(result, error);

  // FAT is case insensitive; host tests and some SD adapters are not. Reuse
  // the actual name found on disk so the original parser opens that same file.
  bool found = false;
  for (const auto& entry : fs::directory_iterator(directory, error)) {
    if (error) break;
    if (EqualsAsciiInsensitive(entry.path().filename().string(), requested.filename().string())) {
      if (found) { result.problem = "ambiguous-target"; return result; }
      found = true;
      result.target = entry.path().string();
      result.target_type = TypeName(entry.status(error).type());
      if (error || result.target_type != "regular") {
        result.problem = "target-not-regular";
        RecordError(result, error);
        return result;
      }
      result.target_readable = Readable(result.target);
      if (!result.target_readable) { result.problem = "target-unreadable"; return result; }
      if (fs::file_size(result.target, error) == 0 || error) {
        result.problem = "target-empty-or-unreadable";
        RecordError(result, error);
        return result;
      }
    }
  }
  if (error) { result.problem = "target-directory-scan"; RecordError(result, error); return result; }
  if (found) { result.used_existing = true; return result; }
  result.target_type = "missing";
  result.source_type = TypeName(fs::status(source, error).type());
  if (error || result.source_type != "regular") {
    result.problem = "template-missing-or-invalid";
    RecordError(result, error);
    return result;
  }
  result.source_readable = Readable(source);
  if (!result.source_readable) { result.problem = "template-unreadable"; return result; }
  return result;
}

inline void Create(Result& result) {
  if (result.ok() || !result.problem.empty()) return;
  std::error_code error;
  const auto& source = result.source;
  const auto& target = result.target;
  result.created = fs::copy_file(source, target, fs::copy_options::none, error);
  RecordError(result, error);
  if (!result.created) result.problem = "copy-failed";
}
}  // namespace srhd_awa::platform::e2e5_cfg_bootstrap

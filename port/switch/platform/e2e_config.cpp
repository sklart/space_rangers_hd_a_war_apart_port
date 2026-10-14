#include "e2e_config.hpp"

#include "game_path.hpp"
#include "user_root.hpp"
#if defined(__SWITCH__)
#include "e2e_stage.hpp"
#endif

#include <cstdio>
#include <filesystem>
#include <sstream>

namespace srhd_awa::platform::e2e_config {
namespace {
namespace fs = std::filesystem;

std::string Describe(const fs::path& path, const char* label) {
  std::error_code ec;
  const auto status = fs::status(path, ec);
  std::ostringstream out;
  out << label << '=' << path.generic_string() << " exists=" << fs::exists(status)
      << " regular=" << fs::is_regular_file(status)
      << " directory=" << fs::is_directory(status)
      << " status_ec=" << ec.value() << ':' << ec.message();
  return out.str();
}

bool Readable(const fs::path& path) {
  if (auto* file = std::fopen(path.generic_string().c_str(), "rb")) {
    const bool ok = std::ferror(file) == 0;
    std::fclose(file);
    return ok;
  }
  return false;
}
}  // namespace

bool EnsureUserConfig(std::string* detail) {
  const fs::path source(game_path::Resolve("cfg.txt"));
  const fs::path target(user_root::ResolveConfigPath("CFG.TXT"));
  std::error_code ec;
  const auto parent = target.parent_path();
  const bool parent_exists = !parent.empty() && fs::is_directory(parent, ec);
  std::ostringstream out;
  out << Describe(source, "source") << " source_readable=" << Readable(source)
      << ' ' << Describe(target, "target") << " target_readable=" << Readable(target)
      << " target_dir=" << parent.generic_string() << " target_dir_exists=" << parent_exists
      << " target_dir_ec=" << ec.value() << ':' << ec.message();
  ec.clear();
  const auto space = fs::space(parent, ec);
  out << " space_ec=" << ec.value() << ':' << ec.message();
  if (!ec) out << " space_available=" << space.available;

  if (target.empty() || !parent_exists) {
    out << " result=invalid-target";
  } else if (fs::is_regular_file(target, ec) && !ec && Readable(target)) {
    out << " result=existing-user-cfg";
    if (detail) *detail = out.str();
    return true;
  } else if (fs::exists(target, ec) || ec) {
    out << " result=invalid-existing-target ec=" << ec.value() << ':' << ec.message();
  } else if (source.empty() || !fs::is_regular_file(source, ec) || ec || !Readable(source)) {
    out << " result=missing-or-unreadable-template ec=" << ec.value() << ':' << ec.message();
  } else {
#if defined(__SWITCH__)
    e2e_stage::LogWinApi(out.str().c_str());
#endif
    ec.clear();
    const bool copied = fs::copy_file(source, target, fs::copy_options::none, ec);
    out << " copy_result=" << copied << " copy_ec=" << ec.value() << ':' << ec.message();
    if (copied && !ec && fs::is_regular_file(target) && Readable(target)) {
      out << " result=created-from-template";
      if (detail) *detail = out.str();
      return true;
    }
    out << " result=copy-failed";
  }
  if (detail) *detail = out.str();
  return false;
}
}  // namespace srhd_awa::platform::e2e_config

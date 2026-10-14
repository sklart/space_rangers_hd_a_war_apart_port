#include "e2e5_cfg_bootstrap.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
namespace cfg = srhd_awa::platform::e2e5_cfg_bootstrap;

static std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main() {
  const auto root = fs::temp_directory_path() / "e2e5-cfg-bootstrap-regression";
  fs::remove_all(root);
  fs::create_directories(root / "game");
  fs::create_directories(root / "user");
  const auto source = root / "game/CfG.TxT";
  const auto target = root / "user/CFG.TXT";
  {
    std::ofstream output(source); output << "[Video]\nSize=1280x720\n";
  }

  // Missing CFG, real template, then a second launch: no replacement.
  auto first = cfg::Inspect(source.string(), target.string());
  assert(first.problem.empty() && !first.used_existing && first.source_readable);
  cfg::Create(first);
  assert(first.created && Read(target) == "[Video]\nSize=1280x720\n");
  auto second = cfg::Inspect(source.string(), target.string());
  assert(second.used_existing && second.target_readable);
  cfg::Create(second);
  assert(!second.created);

  // User settings, including a differently cased filename, are preserved.
  fs::rename(target, root / "user/cfg.txt");
  {
    std::ofstream output(root / "user/cfg.txt"); output << "UserSetting=keep\n";
  }
  auto custom = cfg::Inspect(source.string(), target.string());
  assert(custom.used_existing && custom.target == (root / "user/cfg.txt").string());
  cfg::Create(custom);
  assert(Read(root / "user/cfg.txt") == "UserSetting=keep\n");

  // Neither user CFG nor template: fail without creating a dummy file.
  fs::remove(root / "user/cfg.txt");
  fs::remove(source);
  auto missing = cfg::Inspect(source.string(), target.string());
  assert(!missing.ok() && missing.problem == "template-missing-or-invalid");
  cfg::Create(missing);
  assert(!fs::exists(target));

  // Invalid source and invalid target-directory are distinct failures.
  fs::create_directory(source);
  auto invalid_source = cfg::Inspect(source.string(), target.string());
  assert(invalid_source.problem == "template-missing-or-invalid");
  auto invalid_directory = cfg::Inspect(source.string(), (root / "absent/CFG.TXT").string());
  assert(invalid_directory.problem == "target-directory-missing");

  // A directory in place of CFG must never be overwritten.
  fs::create_directory(target);
  auto invalid_target = cfg::Inspect(source.string(), target.string());
  assert(invalid_target.problem == "target-not-regular");
  fs::remove(target);
  { std::ofstream empty(target); }
  auto empty_target = cfg::Inspect(source.string(), target.string());
  assert(empty_target.problem == "target-empty-or-unreadable");
  fs::remove_all(root);
}

#include "e2e_config.hpp"
#include "game_path.hpp"
#include "user_root.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
namespace platform = srhd_awa::platform;

static void Write(const fs::path& path, const std::string& contents) {
  std::ofstream out(path, std::ios::binary);
  out << contents;
}

static std::string Read(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) return 2;
  const fs::path base(argv[1]);
  const auto game = base / "game";
  const auto user = base / "user";
  fs::create_directories(game);
  platform::game_path::SetRoot(game.string());
  platform::user_root::SetRoot(user.string());
  if (!platform::user_root::EnsureLayout()) return 3;
  std::string detail;
  const auto target = user / "config" / "CFG.TXT";
  const auto source = game / "cFg.TxT";
  if (argc == 3 && std::string(argv[2]) == "reuse") {
    if (!platform::e2e_config::EnsureUserConfig(&detail) ||
        Read(target) != "second-launch-user=99\n" || fs::exists(source)) return 15;
    std::cout << "e2e config second process passed\n";
    return 0;
  }

  // A missing source must not produce an empty placeholder.
  if (platform::e2e_config::EnsureUserConfig(&detail) || fs::exists(target)) return 4;
  if (detail.find("missing-or-unreadable-template") == std::string::npos) return 5;

  Write(source, "template=1\n");
  if (!platform::e2e_config::EnsureUserConfig(&detail) || Read(target) != "template=1\n") return 6;
  if (detail.find("created-from-template") == std::string::npos) return 7;

  Write(target, "user-setting=42\n");
  if (!platform::e2e_config::EnsureUserConfig(&detail) || Read(target) != "user-setting=42\n") return 8;
  if (!platform::e2e_config::EnsureUserConfig(&detail) || Read(target) != "user-setting=42\n") return 9;

  fs::remove(target);
  const auto lowercase = user / "config" / "cfg.txt";
  Write(lowercase, "lowercase-user=7\n");
  if (!platform::e2e_config::EnsureUserConfig(&detail) || fs::exists(target) || Read(lowercase) != "lowercase-user=7\n") return 10;
  if (platform::user_root::ResolveConfigPath("CFG.TXT") != lowercase.generic_string()) return 11;

  fs::remove(lowercase);
  fs::create_directory(target);
  if (platform::e2e_config::EnsureUserConfig(&detail)) return 12;
  fs::remove(target);
  fs::permissions(source, fs::perms::none);
  if (platform::e2e_config::EnsureUserConfig(&detail) || fs::exists(target)) return 14;
  fs::permissions(source, fs::perms::owner_all);
  fs::remove(source);
  if (platform::e2e_config::EnsureUserConfig(&detail) || fs::exists(target)) return 13;

  Write(source, "template=2\n");
  if (!platform::e2e_config::EnsureUserConfig(&detail)) return 16;
  Write(target, "second-launch-user=99\n");
  fs::remove(source);

  std::cout << "e2e config cases passed\n";
}

#include "game_path.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
namespace fs=std::filesystem;
namespace game=srhd_awa::platform::game_path;
int main() {
  const auto root=fs::absolute("build/e2e55/path-fixture");
  fs::create_directories(root/"Mods/Tweaks/Example");
  std::ofstream(root/"Mods/Tweaks/Example/ModuleInfo.txt")<<"Name=Example\n";
  game::SetRoot(root.string());
  assert(game::FileExists("mods/tweaks/example/moduleinfo.txt"));
  assert(game::FileExists((root/"mods/tweaks/example/moduleinfo.txt").string()));
  assert(!game::FileExists((root/"ModuleInfo.txt").string()));
  assert(game::Resolve("../outside").empty());
}

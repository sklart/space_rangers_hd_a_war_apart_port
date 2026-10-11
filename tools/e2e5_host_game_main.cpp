#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include "units/Rangers.hpp"
#include "game_path.hpp"
#include "user_root.hpp"

#include <cstdlib>
#include <filesystem>

// Test-only entrypoint. The original ProgramMain still performs startup and
// runs the game; only its game and user roots are supplied by the host rig.
int main() {
  const auto stage = std::filesystem::current_path();
  const char* installed = std::getenv("E2E5_GAME_ROOT");
  const auto game = installed && *installed
      ? std::filesystem::canonical(installed)
      : stage / "game";
  const auto user = stage / "user";
  if (installed && *installed) {
    const auto relative = std::filesystem::weakly_canonical(stage).lexically_relative(game);
    if (relative.empty() || *relative.begin() != "..") return 2;
  }
  if (!installed || !*installed) std::filesystem::create_directories(game);
  if (std::filesystem::is_symlink(user)) return 3;
  srhd_awa::platform::game_path::SetRoot(game.string());
  srhd_awa::platform::user_root::SetRoot(user.string());
  if (!srhd_awa::platform::user_root::EnsureLayout()) return 4;
  GR_Main::OverrideGameUserDirectory =
      pas::WideString((user / "config").u16string().c_str());
  Rangers::ProgramMain();
  return 0;
}

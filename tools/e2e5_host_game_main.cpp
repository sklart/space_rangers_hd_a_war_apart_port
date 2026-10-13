#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include "units/Rangers.hpp"
#include "game_path.hpp"
#include "user_root.hpp"
#include "win32_compat_messages.hpp"
#include "win32_compat_window.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace {
void DriveOriginalGameKeys(const std::filesystem::path& stage,
                           const std::atomic<bool>& running,
                           bool start_game, bool select_dialog,
                           bool mouse_new_game) {
  const auto log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log";
  constexpr auto kMenu = "screen dispatch id=1 postload=";
  constexpr auto kReady = "screen Run OnOpen ready";
  bool new_game_sent = false;
  bool start_game_sent = false;
  bool introduction_sent = false;
  while (running.load()) {
    std::string lines;
    try {
      std::ifstream stream(log);
      if (stream)
        lines.assign(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
    } catch (const std::ios_base::failure&) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    if (!lines.empty()) {
      const auto menu = lines.find(kMenu);
      const auto settings = lines.find("screen dispatch id=33 postload=");
      const bool ready_to_send = !new_game_sent
          ? menu != std::string::npos && lines.find(kReady, menu) != std::string::npos
          : !start_game_sent
            ? start_game && settings != std::string::npos &&
              lines.find("screen Run frames=60", settings) != std::string::npos
            : !introduction_sent &&
              lines.find("introduction status=continue-enabled") != std::string::npos;
      if (ready_to_send) {
        const auto window = srhd_awa::platform::win32_compat::MainWindow();
        if (window) {
          if (!new_game_sent && mouse_new_game) {
            const auto bounds = lines.find("menu_new_bounds left=");
            int left = 0, top = 0, right = 0, bottom = 0;
            if (bounds == std::string::npos ||
                std::sscanf(lines.c_str() + bounds,
                            "menu_new_bounds left=%d top=%d right=%d bottom=%d",
                            &left, &top, &right, &bottom) != 4 ||
                left >= right || top >= bottom ||
                !srhd_awa::platform::win32_compat::PostHostLeftClick(
                    window, (left + right) / 2, (top + bottom) / 2))
              continue;
          } else {
            srhd_awa::platform::win32_compat::PostHostReturnKey(window);
          }
          if (!new_game_sent) {
            if (!start_game) return;
            new_game_sent = true;
          } else if (!start_game_sent) {
            start_game_sent = true;
          } else {
            introduction_sent = true;
            if (!select_dialog) return;
          }
        }
      }
      if (select_dialog && introduction_sent) {
        constexpr auto kChoice = "dialog choice ready x=";
        const auto choice = lines.rfind(kChoice);
        int x = -1, y = -1;
        if (choice != std::string::npos &&
            std::sscanf(lines.c_str() + choice, "dialog choice ready x=%d y=%d",
                        &x, &y) == 2 && x >= 0 && y >= 0) {
          const auto window = srhd_awa::platform::win32_compat::MainWindow();
          if (window && srhd_awa::platform::win32_compat::PostHostLeftClick(window, x, y))
            return;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
}  // namespace

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
  std::atomic<bool> running{true};
  std::thread new_game_driver;
  const char* drive = std::getenv("E2E5_HOST_AUTO_NEW_GAME");
  const char* start = std::getenv("E2E5_HOST_AUTO_START_GAME");
  const char* select = std::getenv("E2E5_HOST_AUTO_SELECT_DIALOG");
  const char* mouse_new = std::getenv("E2E5_HOST_MOUSE_NEW_GAME");
  if ((drive && *drive == '1') || (start && *start == '1') ||
      (select && *select == '1') || (mouse_new && *mouse_new == '1'))
    new_game_driver = std::thread(DriveOriginalGameKeys, stage,
                                  std::cref(running),
                                  (start && *start == '1') || (select && *select == '1'),
                                  select && *select == '1',
                                  mouse_new && *mouse_new == '1');
  Rangers::ProgramMain();
  running = false;
  if (new_game_driver.joinable()) new_game_driver.join();
  return 0;
}

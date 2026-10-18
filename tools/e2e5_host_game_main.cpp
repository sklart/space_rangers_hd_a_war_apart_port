#include "runtime_support.hpp"
#include "e2e_stage.hpp"
#include "units/GR_Main.hpp"
#include "units/Rangers.hpp"
#include "game_path.hpp"
#include "user_root.hpp"
#include "win32_compat_messages.hpp"
#include "win32_compat_window.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace {
void DriveMenuProbe(const std::filesystem::path& stage,
                    const std::atomic<bool>& running, char key) {
  const auto log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log";
  unsigned step = 0;
  while (running.load()) {
    std::string lines;
    try {
      std::ifstream stream(log);
      if (stream) lines.assign(std::istreambuf_iterator<char>(stream), {});
    } catch (const std::ios_base::failure&) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    const auto window = srhd_awa::platform::win32_compat::MainWindow();
    if (window && step == 0 && lines.find("screen first presented id=1 ") != std::string::npos) {
      srhd_awa::platform::e2e_stage::Log("HOST menu probe begin");
      srhd_awa::platform::win32_compat::PostHostKey(window, key);
      step = 1;
    }
    const auto began = lines.find("HOST menu probe begin");
    const auto panel = lines.rfind("screen dispatch id=40 postload=");
    if (key == 'A' && step == 1 && panel != std::string::npos &&
        lines.find("screen Run frames=60", panel) != std::string::npos) {
      srhd_awa::platform::win32_compat::PostHostKey(window, 0x28);
      srhd_awa::platform::win32_compat::PostHostReturnKey(window);
      step = 2;
    }
    const auto arena = lines.rfind("screen dispatch id=21 postload=");
    const bool done = key == 'M'
        ? began != std::string::npos && lines.find("screen Run OnOpen ready", began) != std::string::npos &&
          lines.find("screen Run frames=60", began) != std::string::npos
        : step == 2 && arena != std::string::npos &&
          lines.find("screen RunContinuous frames=60", arena) != std::string::npos;
    if (done) {
      srhd_awa::platform::e2e_stage::Log("HOST menu probe completed");
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void DriveOriginalOptions(const std::filesystem::path& stage,
                          const std::atomic<bool>& running) {
  const bool save_first = std::getenv("E2E5_HOST_OPTIONS_SAVE") != nullptr;
  const bool save_with_vsync = std::getenv("E2E5_HOST_OPTIONS_SAVE_VSYNC") != nullptr;
  const bool change_audio_volume = std::getenv("E2E5_HOST_OPTIONS_AUDIO") != nullptr;
  const bool toggle_music = std::getenv("E2E5_HOST_OPTIONS_MUSIC") != nullptr;
  const auto log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log";
  unsigned opened = 0;
  unsigned closed = 0;
  unsigned interaction_step = 0;
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
    const auto count = [&](const char* marker) {
      unsigned value = 0;
      for (auto at = lines.find(marker); at != std::string::npos;
           at = lines.find(marker, at + 1)) ++value;
      return value;
    };
    const auto window = srhd_awa::platform::win32_compat::MainWindow();
    const auto menu_at = lines.rfind("screen dispatch id=1 postload=");
    if (window && opened < 2 &&
        count("screen dispatch id=1 postload=") > opened &&
        menu_at != std::string::npos &&
        lines.find("screen Run OnOpen ready", menu_at) != std::string::npos &&
        srhd_awa::platform::win32_compat::PostHostKey(window, 'C')) {
      ++opened;
    }
    const auto options_at = lines.rfind("screen dispatch id=26 postload=");
    const bool options_ready = window && opened > closed &&
        count("screen dispatch id=26 postload=") > closed &&
        options_at != std::string::npos &&
        lines.find("options groups constructed", options_at) != std::string::npos &&
        lines.find("screen Run first frame attempted", options_at) != std::string::npos;
    if (options_ready) {
      const auto current = lines.substr(options_at);
      const auto click_logged_bounds = [&](const std::string& marker) {
        const auto at = current.find(marker);
        if (at == std::string::npos) return false;
        int left{}, top{}, right{}, bottom{};
        if (std::sscanf(current.c_str() + at + marker.size(),
                        "%d,%d,%d,%d", &left, &top, &right, &bottom) != 4 ||
            right <= left || bottom <= top) return false;
        return srhd_awa::platform::win32_compat::PostHostLeftClick(
            window, (left + right) / 2, (top + bottom) / 2);
      };
      const auto group = [&](unsigned index) {
        return click_logged_bounds("options group button=" +
                                   std::to_string(index) + " bounds=");
      };
      if (save_first && !save_with_vsync && closed == 0) {
        if (group(6)) ++closed;
      } else if (closed == 0) {
        if (interaction_step == 0 && group(1)) ++interaction_step;
        else if (interaction_step == 1 &&
                 current.find("options active group=1") != std::string::npos &&
                 click_logged_bounds("options choice target=VSync bounds=")) ++interaction_step;
        else if (interaction_step == 2 &&
                 current.find("options choice selected VSync=") != std::string::npos &&
                 group(2)) ++interaction_step;
        else if (interaction_step == 3 &&
                 current.find("options active group=2") != std::string::npos &&
                 change_audio_volume &&
                 click_logged_bounds("options slider target=MusicVolume bounds="))
          interaction_step = 30;
        else if (interaction_step == 30 &&
                 click_logged_bounds("options slider target=SoundVolume bounds="))
          interaction_step = 31;
        else if ((interaction_step == 3 || interaction_step == 31) && toggle_music &&
                 click_logged_bounds("options choice target=Music bounds="))
          interaction_step = 32;
        else if ((interaction_step == 3 || interaction_step == 31 || interaction_step == 32) &&
                 current.find("options active group=2") != std::string::npos &&
                 group(4)) interaction_step = 4;
        else if (interaction_step == 4 &&
                 current.find("options mode=1") != std::string::npos &&
                 group(1)) ++interaction_step;
        else if (interaction_step == 5 &&
                 current.find("options active group=5") != std::string::npos &&
                 (save_with_vsync ? group(6) :
                     srhd_awa::platform::win32_compat::PostHostKey(window, 0x1b))) {
          ++interaction_step;
          ++closed;
        }
      } else if (srhd_awa::platform::win32_compat::PostHostKey(window, 0x1b)) {
        ++closed;
        if (closed == 2) return;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void DriveOriginalGameKeys(const std::filesystem::path& stage,
                           const std::atomic<bool>& running,
                           bool start_game, bool select_dialog,
                           bool mouse_new_game, bool advance_dialog,
                           bool quicksave, bool takeoff) {
  const auto log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log";
  constexpr auto kMenu = "screen dispatch id=1 postload=";
  constexpr auto kReady = "screen Run OnOpen ready";
  bool new_game_sent = false;
  bool start_game_sent = false;
  bool introduction_sent = false;
  std::size_t last_choice = 0;
  unsigned choices_sent = 0;
  const auto turns_requested = std::getenv("E2E59_HOST_END_TURNS") ?
      std::max(0, std::atoi(std::getenv("E2E59_HOST_END_TURNS"))) : 0;
  unsigned turns_sent = 0;
  bool takeoff_sent = false;
  bool first_map_orders_seen = false;
  bool swipe_sent = false;
  unsigned cancel_stage = 0, cancel_turn = 0, cancel_taps = 0, cancel_releases = 0;
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
      const bool awaiting_music_fault = start_game_sent && !introduction_sent &&
          std::getenv("E2E511_HOST_MUSIC_OOM") &&
          (lines.find("HOST music file worker injected bad_alloc generation_stage=8") == std::string::npos ||
           lines.find("worker exception class=N13EC_FileStream13TFileStreamECE") == std::string::npos);
      const auto ui_snapshot = lines.find("introduction ui_snapshot ");
      const bool awaiting_ui_snapshots = start_game_sent && !introduction_sent &&
          std::getenv("E2E511_HOST_WAIT_UI_SNAPSHOTS") &&
          (ui_snapshot == std::string::npos ||
           lines.find("introduction ui_snapshot ", ui_snapshot + 1) == std::string::npos);
      if (ready_to_send && !awaiting_music_fault && !awaiting_ui_snapshots) {
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
          } else if (start_game_sent && std::getenv("E2E511_HOST_TOUCH_CONTINUE")) {
            const auto bounds = lines.find("introduction status=continue-enabled left=");
            int left{}, top{}, right{}, bottom{};
            if (bounds == std::string::npos ||
                std::sscanf(lines.c_str() + bounds,
                    "introduction status=continue-enabled left=%d top=%d right=%d bottom=%d",
                    &left, &top, &right, &bottom) != 4 ||
                left >= right || top >= bottom) continue;
            // Exercise the same normalization, touch provenance and message
            // queue used on Switch, before the real GI hit test and callback.
            srhd_awa::platform::input_platform::RawInput touch{};
            touch.touch = true;
            touch.touch_x = (left + right) / 2;
            touch.touch_y = (top + bottom) / 2;
            srhd_awa::platform::e2e_stage::Log("HOST introduction raw touch begin");
            srhd_awa::platform::win32_compat::InjectHostInput(touch);
            // A latest-state host source has no physical HID history. Do not
            // release a pulse before a busy UI has sampled its down edge.
            bool down_observed = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (running.load() && std::chrono::steady_clock::now() < deadline) {
              std::ifstream observed(log);
              const std::string snapshot{std::istreambuf_iterator<char>(observed),
                                         std::istreambuf_iterator<char>()};
              const auto begin = snapshot.rfind("HOST introduction raw touch begin");
              if (begin != std::string::npos && snapshot.find(
                    "introduction input phase=return message=513", begin) != std::string::npos) {
                down_observed = true;
                break;
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            srhd_awa::platform::win32_compat::InjectHostInput({});
            srhd_awa::platform::e2e_stage::Log("HOST introduction raw touch release");
            if (!down_observed) {
              srhd_awa::platform::e2e_stage::Log("FAIL HOST introduction raw touch down not observed");
              return;
            }
          } else if (start_game_sent && std::getenv("E2E511_HOST_MOUSE_CONTINUE")) {
            const auto bounds = lines.find("introduction status=continue-enabled left=");
            int left{}, top{}, right{}, bottom{};
            if (bounds == std::string::npos ||
                std::sscanf(lines.c_str() + bounds,
                    "introduction status=continue-enabled left=%d top=%d right=%d bottom=%d",
                    &left, &top, &right, &bottom) != 4 ||
                left >= right || top >= bottom ||
                !srhd_awa::platform::win32_compat::PostHostLeftClick(
                    window, (left + right) / 2, (top + bottom) / 2)) continue;
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
        if (turns_requested) {
          const auto dispatch = lines.rfind("screen dispatch id=");
          const auto target = takeoff ? "screen dispatch id=16 " : "screen dispatch id=4 ";
          unsigned completed = 0;
          for (auto offset = lines.find("turn operation=end-turn-ui"); offset != std::string::npos;
               offset = lines.find("turn operation=end-turn-ui", offset + 1)) ++completed;
          if (takeoff) {
            completed = 0;
            for (auto offset = lines.find("HOST map orders ready"); offset != std::string::npos;
                 offset = lines.find("HOST map orders ready", offset + 1)) ++completed;
            if (completed && !first_map_orders_seen) {
              first_map_orders_seen = true;
              turns_sent = 0; // A Space during the takeoff film only requests a break.
            }
            if (completed) --completed; // First order-mode entry is the initial map, not a completed day.
          }
          if (dispatch != std::string::npos && lines.compare(dispatch, std::strlen(target), target) == 0 &&
              lines.find("frames=60", dispatch) != std::string::npos) {
            if (takeoff && first_map_orders_seen && std::getenv("E2E59_HOST_MAP_SWIPE")) {
              if (!swipe_sent) {
                srhd_awa::platform::input_platform::RawInput touch{};
                touch.touch = true; touch.touch_x = 500; touch.touch_y = 350;
                srhd_awa::platform::win32_compat::InjectHostInput(touch);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                touch.touch_x = 620; touch.touch_y = 420;
                srhd_awa::platform::win32_compat::InjectHostInput(touch);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                srhd_awa::platform::win32_compat::InjectHostInput({});
                swipe_sent = true;
                continue;
              }
              if (lines.find("HOST native map swipe moved=1 ship_order_preserved=1") == std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
              }
            }
            if (takeoff && first_map_orders_seen && std::getenv("E2E59_HOST_MAP_CANCEL") && cancel_stage < 3) {
              const auto count = [&](const char* marker) {
                unsigned result = 0;
                for (auto pos = lines.find(marker); pos != std::string::npos; pos = lines.find(marker, pos + 1)) ++result;
                return result;
              };
              if (cancel_stage == 0) {
                cancel_taps = count("HOST native map tap committed");
                srhd_awa::platform::input_platform::RawInput touch{};
                touch.touch = true; touch.touch_x = 500; touch.touch_y = 350;
                srhd_awa::platform::win32_compat::InjectHostInput(touch);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                const auto window = srhd_awa::platform::win32_compat::MainWindow();
                if (window && srhd_awa::platform::win32_compat::PostHostKey(window, 0x20)) {
                  cancel_turn = completed + 1; turns_sent = cancel_turn; cancel_stage = 1;
                }
              } else if (cancel_stage == 1 && completed >= cancel_turn) {
                cancel_releases = count("HOST native map touch release without capture");
                srhd_awa::platform::win32_compat::InjectHostInput({});
                cancel_stage = 2;
              } else if (cancel_stage == 2 && count("HOST native map touch release without capture") > cancel_releases) {
                if (count("HOST native map tap committed") == cancel_taps &&
                    lines.find("HOST native map touch canceled by mode change") != std::string::npos) {
                  srhd_awa::platform::e2e_stage::LogE2E3("HOST native canceled gesture released without tap PASS");
                  cancel_stage = 3;
                } else {
                  srhd_awa::platform::e2e_stage::LogE2E3("HOST native canceled gesture FAILED");
                  return;
                }
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
              continue;
            }
            if (completed >= static_cast<unsigned>(turns_requested)) return;
            if (completed >= turns_sent) {
              const auto window = srhd_awa::platform::win32_compat::MainWindow();
              if (window && srhd_awa::platform::win32_compat::PostHostKey(window, 0x20)) ++turns_sent;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
          }
        }
        if (takeoff) {
          const auto hangar = lines.find("screen dispatch id=4 postload=");
          if (!takeoff_sent && hangar != std::string::npos &&
              lines.find("screen Run frames=60", hangar) != std::string::npos) {
            const auto window = srhd_awa::platform::win32_compat::MainWindow();
            // Position taken from the supplied 1280x720 hardware input trace.
            if (window && srhd_awa::platform::win32_compat::PostHostLeftClick(
                              window, 664, 597)) {
              if (!turns_requested) return;
              takeoff_sent = true;
            }
          }
        }
        if (quicksave) {
          const auto hangar = lines.find("screen dispatch id=4 postload=");
          if (hangar != std::string::npos &&
              lines.find("screen Run frames=60", hangar) != std::string::npos) {
            const auto window = srhd_awa::platform::win32_compat::MainWindow();
            if (window && srhd_awa::platform::win32_compat::PostHostKey(window, 0x74))
              return;  // VK_F5, original TfPanelMain::QuickSave shortcut.
          }
        }
        if (advance_dialog && lines.find("screen returned id=20") != std::string::npos) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }
        if (choices_sent >= (advance_dialog ? 8u : 1u)) return;
        const auto callbacks = [&lines] {
          constexpr auto kCallback = "dialog choice callback complete";
          unsigned count = 0;
          for (auto offset = lines.find(kCallback); offset != std::string::npos;
               offset = lines.find(kCallback, offset + 1)) ++count;
          return count;
        }();
        if (callbacks < choices_sent) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }
        constexpr auto kChoice = "dialog choice ready x=";
        const auto choice = lines.rfind(kChoice);
        int x = -1, y = -1;
        if (choice != std::string::npos && choice > last_choice &&
            std::sscanf(lines.c_str() + choice, "dialog choice ready x=%d y=%d",
                        &x, &y) == 2 && x >= 0 && y >= 0) {
          const auto window = srhd_awa::platform::win32_compat::MainWindow();
          if (window && srhd_awa::platform::win32_compat::PostHostLeftClick(window, x, y)) {
            last_choice = choice;
            ++choices_sent;
            if (!advance_dialog) return;
          }
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void DriveOriginalGameLoad(const std::filesystem::path& stage,
                           const std::atomic<bool>& running) {
  const auto log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log";
  bool menu_clicked = false;
  bool save_clicked = false;
  bool ship_open_requested = false;
  bool ship_close_requested = false;
  auto ship_closed_at = std::chrono::steady_clock::time_point{};
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
    if (save_clicked) {
      if (ship_open_requested) {
        const auto open = lines.find("ship_equipment_open");
        if (!ship_close_requested && open != std::string::npos &&
            lines.find("screen Run frames=60", open) != std::string::npos) {
          const auto window = srhd_awa::platform::win32_compat::MainWindow();
          if (window && srhd_awa::platform::win32_compat::PostHostKey(window, 0x1b))
            ship_close_requested = true;  // VK_ESCAPE in original ship screen.
        }
        if (ship_close_requested) {
          const auto closed = lines.find("ship_equipment_closed", open);
          if (closed != std::string::npos &&
              ship_closed_at == std::chrono::steady_clock::time_point{})
            ship_closed_at = std::chrono::steady_clock::now();
          if (ship_closed_at != std::chrono::steady_clock::time_point{} &&
              std::chrono::steady_clock::now() - ship_closed_at >=
                  std::chrono::seconds(2)) {
            const auto window = srhd_awa::platform::win32_compat::MainWindow();
            if (window && srhd_awa::platform::win32_compat::PostHostKey(window, 0x20))
              return;  // VK_SPACE in original Hangar runs a game turn.
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      const auto load = lines.find("screen dispatch id=24 postload=");
      const auto hangar = load == std::string::npos ? load :
          lines.find("screen dispatch id=4 postload=", load);
      if (hangar != std::string::npos &&
          lines.find("screen Run frames=60", hangar) != std::string::npos) {
        const auto window = srhd_awa::platform::win32_compat::MainWindow();
        if (window && srhd_awa::platform::win32_compat::PostHostKey(window, 'S'))
          ship_open_requested = true;  // Original Hangar shortcut.
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    const auto dispatch = lines.find(menu_clicked ? "screen dispatch id=23 postload="
                                                  : "screen dispatch id=1 postload=");
    const auto bounds = lines.find(menu_clicked ? "save_load_bounds left="
                                                : "menu_load_bounds left=", dispatch);
    if (dispatch != std::string::npos && bounds != std::string::npos &&
        lines.find("screen Run frames=60", dispatch) != std::string::npos) {
      int left = 0, top = 0, right = 0, bottom = 0;
      const char* pattern = menu_clicked
          ? "save_load_bounds left=%d top=%d right=%d bottom=%d"
          : "menu_load_bounds left=%d top=%d right=%d bottom=%d";
      if (std::sscanf(lines.c_str() + bounds, pattern,
                      &left, &top, &right, &bottom) == 4 &&
          left < right && top < bottom) {
        const auto window = srhd_awa::platform::win32_compat::MainWindow();
        if (window && srhd_awa::platform::win32_compat::PostHostLeftClick(
                          window, (left + right) / 2, (top + bottom) / 2)) {
          if (menu_clicked) {
            save_clicked = true;
            continue;
          }
          menu_clicked = true;
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
  const char* advance = std::getenv("E2E5_HOST_AUTO_ADVANCE_DIALOG");
  const char* quicksave = std::getenv("E2E5_HOST_AUTO_QUICKSAVE");
  const char* takeoff = std::getenv("E2E5_HOST_AUTO_TAKEOFF");
  const char* mouse_new = std::getenv("E2E5_HOST_MOUSE_NEW_GAME");
  const char* load_save = std::getenv("E2E5_HOST_LOAD_SAVE");
  const char* options = std::getenv("E2E5_HOST_AUTO_OPTIONS");
  const char* menu_probe = std::getenv("E2E5_HOST_MENU_PROBE");
  if (menu_probe && (*menu_probe == 'M' || *menu_probe == 'A'))
    new_game_driver = std::thread(DriveMenuProbe, stage, std::cref(running), *menu_probe);
  else if (load_save && *load_save == '1')
    new_game_driver = std::thread(DriveOriginalGameLoad, stage, std::cref(running));
  else if (options && *options == '1')
    new_game_driver = std::thread(DriveOriginalOptions, stage, std::cref(running));
  else if ((drive && *drive == '1') || (start && *start == '1') ||
      (select && *select == '1') || (mouse_new && *mouse_new == '1') ||
      (advance && *advance == '1') || (quicksave && *quicksave == '1') ||
      (takeoff && *takeoff == '1'))
    new_game_driver = std::thread(DriveOriginalGameKeys, stage,
                                  std::cref(running),
                                  (start && *start == '1') || (select && *select == '1') ||
                                      (advance && *advance == '1') || (quicksave && *quicksave == '1') ||
                                      (takeoff && *takeoff == '1'),
                                  (select && *select == '1') || (advance && *advance == '1') ||
                                      (quicksave && *quicksave == '1') || (takeoff && *takeoff == '1'),
                                  mouse_new && *mouse_new == '1',
                                  (advance && *advance == '1') || (quicksave && *quicksave == '1') ||
                                      (takeoff && *takeoff == '1'),
                                  quicksave && *quicksave == '1',
                                  takeoff && *takeoff == '1');
  Rangers::ProgramMain();
  running = false;
  if (new_game_driver.joinable()) new_game_driver.join();
  if (!srhd_awa::platform::e2e_stage::FirstFailure().empty()) {
    srhd_awa::platform::e2e_stage::LogBootFailed();
    std::_Exit(1);
  }
  if (!srhd_awa::platform::e2e_stage::BootReady()) {
    srhd_awa::platform::e2e_stage::Log("BOOT RETURNED EARLY");
    return 1;
  }
  return 0;
}

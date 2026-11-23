// SPDX-License-Identifier: GPL-2.0-or-later
// Runs the original load/main-menu dispatcher with scripted input. No GPU,
// physical HID, campaign, save-file publication, or battle is claimed.
#include "rangers_host_session.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_native_input.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "rangers_screen_loop.hpp"
#include "rangers_world_shutdown.hpp"
#include "native_ui_target.hpp"
#include "ec_file_adapter.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aGalaxy.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

int main(int argc, char** argv) {
    assert(argc == 3);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    namespace m = srhd_awa::matrixgame;
    srhd_awa::platform::ec_file::SetGameRoot(argv[1]);
    srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
    {
        m::RangersHostOptions options; options.sound = false; options.music = true;
        m::RangersHostSession host(options);
        m::RangersUiContext context;
        bool moved = false, quit_sent = false, confirm_sent = false;
        unsigned load_keys = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
        auto last_load_key = std::chrono::steady_clock::time_point{};
        m::RangersNativeInputBackend backend;
        backend.poll = [&] {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            const auto now = std::chrono::steady_clock::now();
            if (now > deadline) throw std::runtime_error("initial screen scenario timed out");
            auto* target = m::native_input::CurrentUiTarget();
            assert(target);
            if (!moved) { target->Move(640, 360); moved = true; }
            const int depth = GI_MessageLoop::MessageLoopStack ? pas::list_count(GI_MessageLoop::MessageLoopStack) : 0;
            if (GlobalsV::CurrentScreenId == GlobalsV::screenLoad && now-last_load_key > 500ms) {
                // Exercise the original intro skip command, not a fake exit.
                target->Key(true, 27); target->Key(false, 27); last_load_key = now; ++load_keys;
            } else if (GlobalsV::CurrentScreenId == GlobalsV::screenMainMenu && depth == 1 && !quit_sent) {
                quit_sent = true; target->Key(true, 27); target->Key(false, 27);
            } else if (GlobalsV::CurrentScreenId == GlobalsV::screenMainMenu && depth > 1 && quit_sent && !confirm_sent) {
                confirm_sent = true; target->Key(true, 'Y'); target->Key(false, 'Y');
            }
        };
        backend.wait_for_drawable = [] {};
        backend.reset_input = [] {};
        backend.drawable = [] { return true; };
        backend.extent = [] { return std::pair{1280, 720}; };
        backend.warp = [](int, int) {};
        m::RangersNativeInput input(backend);
        m::RangersScriptSession scripts;
        m::RangersGlobalUiSession ui;
        const auto result = m::RunInitialRangersScreens();
        assert(result == m::RangersScreenExit::Finished && quit_sent && confirm_sent);
        assert(!aGalaxy::Galaxy && GlobalsV::RequestedScreenId == GlobalsV::screenNone);
        m::ShutdownRangersWorld(scripts);
        std::printf("INITIAL_SCREENS original_load_menu_confirm_exit=PASS scripted_input=YES load_keys=%u GPU=NOT_RUN battle=NOT_RUN\n", load_keys);
    }
    std::puts("INITIAL_SCREENS original_owners_cleanup=PASS");
}

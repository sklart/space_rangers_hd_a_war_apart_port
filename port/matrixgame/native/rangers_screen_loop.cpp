// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_screen_loop.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/aGalaxy.hpp"
#include "rangers_ui_input.hpp"
#include <stdexcept>

namespace srhd_awa::matrixgame {
RangersScreenExit RunInitialRangersScreens() {
    if (!Globals::LoadScreen || !Globals::MainMenuScreen || !Globals::TurnCalculationThread ||
        !srhd_awa::platform::rangers_ui_input::Current())
        throw std::logic_error("initial screen loop requires live original UI, scripts and native input");
    if (aGalaxy::Galaxy || GR_Main::ExitScreenLoop || GlobalsV::MemorySnapshotActive ||
        GlobalsV::CurrentScreenId != GlobalsV::screenNone ||
        GlobalsV::RequestedScreenId != GlobalsV::screenNone ||
        GlobalsV::PostLoadScreenId != GlobalsV::screenNone ||
        (GI_MessageLoop::MessageLoopStack && pas::list_count(GI_MessageLoop::MessageLoopStack)))
        throw std::logic_error("initial screen loop requires an idle process without a campaign");

    // Original Rangers::ProgramMain startup state, before the original
    // dispatcher. No new campaign or battle result is synthesized here.
    GlobalsV::PostLoadScreenId = GlobalsV::screenMainMenu;
    GlobalsV::RequestedScreenId = GlobalsV::screenLoad;
    Globals::ScreenLoadMode = 0;
    Globals::RunMainScreenStateLoop();
    if (!GR_Main::ExitScreenLoop &&
        (GlobalsV::RequestedScreenId != GlobalsV::screenNone ||
         GlobalsV::PostLoadScreenId != GlobalsV::screenNone))
        return RangersScreenExit::RuntimeReloadRequested;
    return RangersScreenExit::Finished;
}
}

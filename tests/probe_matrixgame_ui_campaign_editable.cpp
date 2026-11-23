// SPDX-License-Identifier: GPL-2.0-or-later
// Real generated campaign and original in-memory editable-state producer.
// This does not call the save-file publisher or run a planetary battle.
#include "rangers_host_session.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "rangers_campaign_session.hpp"
#include "rangers_world_shutdown.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include "units/aRanger.hpp"
#include "units/aScript.hpp"
#include "units/aSaveLoad.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/fGov.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>

namespace m = srhd_awa::matrixgame;
#include "matrixgame_campaign_navigation.inc"

int main(int argc, char** argv) {
    assert(argc == 3);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    srhd_awa::platform::ec_file::SetGameRoot(argv[1]);
    srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
    {
        m::RangersHostOptions options; options.sound = false; options.music = false;
        m::RangersHostSession host(options);
        m::RangersUiContext context;
        m::RangersScriptSession scripts;
        m::RangersGlobalUiSession ui;
        const auto previous_screen = GlobalsV::CurrentScreenId;
        GlobalsV::CurrentScreenId = GlobalsV::screenIntroduction;
        {
            m::RangersCampaignSession campaign;
            auto& galaxy = campaign.Galaxy();
            assert(aPlayer::GetPlayer() && GR_Main::EditableSaveBlock);
            const auto day_key = EC_Str::DecodeTextW(u"ImDeary"_w);
            for (int iteration = 0; iteration != 2; ++iteration) {
                galaxy.SaveEditableState();
                assert(GR_Main::EditableSaveBlock->CountParams(day_key) == 1);
                assert(GR_Main::EditableSaveBlock->GetParam(pas::view(day_key)) == pas::wide_int_to_str(galaxy.CurrentTurn));
                assert(GR_Main::EditableSaveBlock->GetBlockCount() > 0);
            }
            std::puts("UI_CAMPAIGN original_generation_and_editable_state_twice=PASS file_publish=NOT_RUN battle=NOT_RUN");
            ExerciseOriginalLanding();
            auto* gov = Globals::GovernmentScreen;
            gov->InitializeLayout();
            const auto turn = campaign.Galaxy().CurrentTurn;
            const auto player_id = aPlayer::GetPlayer()->Id;
            const auto money = aPlayer::GetPlayer()->Money;
            const auto planet_id = aPlayer::GetPlayer()->CurrentPlanet->Id;
            auto* previous_stack = GI_MessageLoop::MessageLoopStack;
            GlobalsV::CurrentScreenId = GlobalsV::screenGovernment;
            GI_MessageLoop::PushMessageLoop(gov);
            gov->OnOpen();
            aSaveLoad::SaveGameToMemorySnapshot();
            auto* snapshot = aSaveLoad::MemorySnapshotBuffer;
            auto* worker = aScript::ScriptRequestThread;
            assert(GlobalsV::MemorySnapshotActive && snapshot && !aGalaxy::Galaxy);
            bool rejected = false;
            try { m::ShutdownRangersWorld(scripts); }
            catch (const std::logic_error&) { rejected = true; }
            assert(rejected && aSaveLoad::MemorySnapshotBuffer == snapshot &&
                   aScript::ScriptRequestThread == worker && GlobalsV::MemorySnapshotActive);
            assert(m::RestorePendingRangersSnapshot());
            assert(!m::RestorePendingRangersSnapshot());
            assert(!GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer &&
                   campaign.Galaxy().CurrentTurn == turn && aPlayer::GetPlayer()->Id == player_id &&
                   aPlayer::GetPlayer()->Money == money && aPlayer::GetPlayer()->CurrentPlanet &&
                   aPlayer::GetPlayer()->CurrentPlanet->Id == planet_id);
            gov->OnClose();
            GI_MessageLoop::PopMessageLoop(gov);
            if (!previous_stack) {
                assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 0);
                pas::free(GI_MessageLoop::MessageLoopStack);
                GI_MessageLoop::MessageLoopStack = nullptr;
            }
            GlobalsV::CurrentScreenId = GlobalsV::screenIntroduction;
            std::puts("UI_CAMPAIGN original_snapshot_guard_and_restore=PASS");
            // End request production, then join the original request worker
            // while both the real world and screen owners remain alive.
            aScript::StartScriptRequestThread();
            assert(Globals::GovernmentScreen && Globals::MainMenuScreen);
            m::ShutdownRangersWorld(scripts);
            assert(!aScript::ScriptRequestThread && !aGalaxy::Galaxy && !aPlayer::GetPlayer());
            m::ShutdownRangersWorld(scripts); // A second terminal cleanup is harmless.
            std::puts("UI_CAMPAIGN request_worker_join_before_world_cleanup=PASS");
        }
        assert(!aGalaxy::Galaxy && !aPlayer::GetPlayer() && !Globals::NewGameGenerationThread && !aRanger::PlayerOldQuests);
        GlobalsV::CurrentScreenId = previous_screen;
    }
    assert(!GR_Main::EditableSaveBlock && !GR_Main::MainDataConfig && !Globals::TurnCalculationThread);
    std::puts("UI_CAMPAIGN original_world_and_owners_cleanup=PASS");
}

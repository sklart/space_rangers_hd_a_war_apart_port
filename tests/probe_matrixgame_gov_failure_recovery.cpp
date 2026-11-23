// SPDX-License-Identifier: GPL-2.0-or-later
// Original Gov/FRun failure recovery with a deliberately failing Robot ABI.
// This does not execute MatrixGame, Switch GPU or publish a save file.
#include "rangers_host_session.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "rangers_campaign_session.hpp"
#include "rangers_world_shutdown.hpp"
#include "rangers_native_input.hpp"
#include "rangers_exception_boundary.hpp"
#include "native_ui_target.hpp"
#include "planet_battle_presentation.hpp"
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
#include "units/Robot.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <thread>
#ifdef GOV_FAILURE_TRACE_THROWS
#include <execinfo.h>
#include <cstring>
#include <typeinfo>
extern "C" [[noreturn]] void __real___cxa_throw(void*,void*,void(*)(void*));
extern "C" [[noreturn]] void __wrap___cxa_throw(void* object,void* info,void(*destroy)(void*)){
 if(*static_cast<std::type_info*>(info)==typeid(std::runtime_error) &&
    std::strcmp(static_cast<std::runtime_error*>(object)->what(),"COM call through nil interface")==0){
  void* frames[40];const int count=backtrace(frames,40);std::fputs("GOV_FAILURE nil_COM_throw_backtrace\n",stderr);backtrace_symbols_fd(frames,count,2);
 }
 __real___cxa_throw(object,info,destroy);
}
#endif

namespace m = srhd_awa::matrixgame;
namespace battle=srhd_awa::platform::planet_battle;
struct TestPresentation final:battle::Provider {
 int captures{},entries{},restores{};
 struct Lease final:battle::Lease {
  TestPresentation& owner;
  explicit Lease(TestPresentation& value):owner(value){}
  void EnterBattle()override{++owner.entries;}
  void Restore()override{++owner.restores;}
  bool UsesNativeWindow()const noexcept override{return true;}
 };
 std::unique_ptr<battle::Lease> Capture()override{++captures;return std::make_unique<Lease>(*this);}
};
int failures{};
std::int32_t PAS_STDCALL FailBattle(std::uint32_t,std::uint32_t,char16_t*,Robot::PRobotDisplaySettings,char16_t*,char16_t*,char16_t*,char16_t*,char16_t*,aGalaxyStruct::PPlanetBattleStatistics){
 return m::TranslateRangersExceptions([]() -> std::int32_t {++failures;std::puts("GOV_FAILURE injected_callback=REACHED");throw std::runtime_error("injected native battle failure");});
}
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
        bool confirm_sent=false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(12);
        m::RangersNativeInputBackend backend;
        backend.poll=[&]{
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Gov failure dialog timeout");
            if(!confirm_sent && GI_MessageLoop::MessageLoopStack && pas::list_count(GI_MessageLoop::MessageLoopStack)>1){
                auto* target=m::native_input::CurrentUiTarget();assert(target);confirm_sent=true;std::puts("GOV_FAILURE confirm_input=SENT");
                target->Key(true,'Y');target->Key(false,'Y');
            }
        };
        backend.wait_for_drawable=[]{};backend.reset_input=[]{};backend.drawable=[]{return true;};
        backend.extent=[]{return std::pair{1280,720};};backend.warp=[](int,int){};
        m::RangersNativeInput input(backend);
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
            assert(!Robot::RobotInterface && Globals::RobotMapDefinitions.length()>0);
            Robot::TRobotInterfacePrefix failing{};failing.Run=Robot::TRobotRun(&FailBattle);Robot::RobotInterface=&failing;
            gov->PlanetBattleMapId=Globals::RobotMapDefinitions[0].Id;fGov::GovernmentBattleDifficulty=1;gov->PendingTransition=1;
            TestPresentation presentation;battle::Scope presentation_scope(presentation);
            gov->OnOpen();
            assert(failures==1 && confirm_sent && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
            assert(presentation.captures==1 && presentation.entries==1 && presentation.restores==1);
            assert(aGalaxy::Galaxy && aPlayer::GetPlayer() && aGalaxy::Galaxy->CurrentTurn==turn &&
                   aPlayer::GetPlayer()->Id==player_id && aPlayer::GetPlayer()->Money==money &&
                   aPlayer::GetPlayer()->CurrentPlanet && aPlayer::GetPlayer()->CurrentPlanet->Id==planet_id);
            assert(!GR_Main::RobotBattleActive);Robot::RobotInterface=nullptr;
            std::puts("GOV_FAILURE original_FRun_exception_snapshot_restore=PASS injected_failure=YES battle=NOT_RUN");
            gov->OnClose();
            GI_MessageLoop::PopMessageLoop(gov);
            if (!previous_stack) {
                assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 0);
                pas::free(GI_MessageLoop::MessageLoopStack);
                GI_MessageLoop::MessageLoopStack = nullptr;
            }
            GlobalsV::CurrentScreenId = GlobalsV::screenIntroduction;
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

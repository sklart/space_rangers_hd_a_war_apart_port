// SPDX-License-Identifier: GPL-2.0-or-later
// Original dispatcher, new-game handlers and file save/reload in an isolated
// host root. This is not a Switch GPU/HID or MatrixGame battle test.
#include "rangers_host_session.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_native_input.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "rangers_achievement_session.hpp"
#include "rangers_screen_loop.hpp"
#include "rangers_world_shutdown.hpp"
#include "native_ui_target.hpp"
#include "ec_file_adapter.hpp"
#include "e2e4_quicksave_transaction.hpp"
#include "runtime_support.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/GI_GraphButton.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/MessageText.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include "units/aSaveLoad.hpp"
#include "units/WindowsSdk.hpp"
#include "types/aSaveLoad.hpp"
#include "types/aPlanet.hpp"
#include "types/fMainForm.hpp"
#include "types/fGameSettings2.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);
 namespace m=srhd_awa::matrixgame;
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 {
  m::RangersHostOptions options;options.sound=false;options.music=true;
  m::RangersHostSession host(options);m::RangersUiContext context;m::RangersAchievementSession achievements;
  assert(!MessageText::QuestMessages);
  auto* messages=pas::construct_call<MessageText::TQuestMessages>(MessageText::TQuestMessages_Create);
  MessageText::QuestMessages=messages;
  {
   bool main_clicked=false,start_clicked=false,planet_seen=false;
   auto last_input=std::chrono::steady_clock::time_point{};
   const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(12);
   m::RangersNativeInputBackend backend;
   backend.poll=[&]{
    using namespace std::chrono_literals;std::this_thread::sleep_for(1ms);
    const auto now=std::chrono::steady_clock::now();if(now>deadline)throw std::runtime_error("menu campaign timed out");
    auto* target=m::native_input::CurrentUiTarget();assert(target);
    if(GlobalsV::CurrentScreenId==GlobalsV::screenLoad && now-last_input>500ms){target->Key(true,27);target->Key(false,27);last_input=now;}
    else if(GlobalsV::CurrentScreenId==GlobalsV::screenMainMenu && !main_clicked){main_clicked=true;std::puts("MENU_CAMPAIGN main_menu=REACHED");Globals::MainMenuScreen->NewGameClicked(nullptr);}
    else if(GlobalsV::CurrentScreenId==GlobalsV::screenNewGame && !start_clicked){
     auto* ok=pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(Globals::NewGameScreen->GetByName(u"Ok"sv));
     if(!ok->Disabled){start_clicked=true;std::puts("MENU_CAMPAIGN new_game_settings=REACHED");Globals::NewGameScreen->ApplyClicked(nullptr);}
    }else if(GlobalsV::CurrentScreenId==GlobalsV::screenIntroduction && aGalaxy::Galaxy && !Globals::NewGameGenerationThread && now-last_input>500ms){
     target->Key(true,13);target->Key(false,13);last_input=now;
    }else if(GlobalsV::CurrentScreenId==GlobalsV::screenPlanet || GlobalsV::CurrentScreenId==GlobalsV::screenPlanetNO || GlobalsV::CurrentScreenId==GlobalsV::screenRuinsTalk){
     assert(aGalaxy::Galaxy && aPlayer::GetPlayer());planet_seen=true;GR_Main::ExitScreenLoop=true;
    }
   };
   backend.wait_for_drawable=[]{};backend.reset_input=[]{};backend.drawable=[]{return true;};
   backend.extent=[]{return std::pair{1280,720};};backend.warp=[](int,int){};
   m::RangersNativeInput input(backend);m::RangersScriptSession scripts;m::RangersGlobalUiSession ui;
   const auto result=m::RunInitialRangersScreens();
   assert(result==m::RangersScreenExit::Finished && main_clicked && start_clicked && planet_seen);
   assert(aGalaxy::Galaxy && aPlayer::GetPlayer() && aPlayer::GetPlayer()->CurrentStar);
   std::puts("MENU_CAMPAIGN menu_settings_generation_planet=PASS handler_driven=YES GPU=NOT_RUN battle=NOT_RUN");
   const auto turn=aGalaxy::Galaxy->CurrentTurn;
   const auto id=aPlayer::GetPlayer()->Id;const auto money=aPlayer::GetPlayer()->Money;
   const auto planet=aPlayer::GetPlayer()->CurrentPlanet ? aPlayer::GetPlayer()->CurrentPlanet->Id : 0;
   const auto path=pas::concat_wide({GR_Main::GetGameUserDirectory(),u"Save\\IntegrationProbe.sav"});
   assert(aSaveLoad::SaveWriter && aSaveLoad::SaveGameToFile(path,u"isolated integration test"_w));
   aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
   assert(srhd_awa::platform::e2e4_quicksave::GameSaveCommitted());
   assert(srhd_awa::platform::e2e4_quicksave::GameSavePathExists(path.pchar()));
   assert(aSaveLoad::LoadGameFromFile(path));
   assert(aGalaxy::Galaxy && aPlayer::GetPlayer() && aGalaxy::Galaxy->CurrentTurn==turn &&
          aPlayer::GetPlayer()->Id==id && aPlayer::GetPlayer()->Money==money &&
          (aPlayer::GetPlayer()->CurrentPlanet ? aPlayer::GetPlayer()->CurrentPlanet->Id : 0)==planet);
   std::puts("MENU_CAMPAIGN original_file_save_and_reload=PASS isolated_root=YES");
   m::ShutdownRangersWorld(scripts);
  }
  MessageText::QuestMessages=nullptr;pas::free(messages);
 }
 std::puts("MENU_CAMPAIGN owners_cleanup=PASS");
}

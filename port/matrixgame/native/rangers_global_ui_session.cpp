// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_global_ui_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Expression.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aGalaxy.hpp"
#include "units/aScript.hpp"
#include "units/aSaveLoad.hpp"
#include "units/PopUp.hpp"
#include "types/fAchievements.hpp"
#include "types/GI_MessageLoop.hpp"
#include <array>
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
bool owned{};
std::array<void*,36> Screens(){return {
 Globals::MainMenuScreen,Globals::NewGameScreen,Globals::IntroductionScreen,Globals::HangarScreen,Globals::PlanetScreen,
 Globals::UninhabitedPlanetScreen,Globals::PlanetQuestScreen,Globals::RuinsTalkScreen,Globals::ArcadeBattleScreen,
 Globals::EquipmentShopScreen,Globals::GoodsShopScreen,Globals::GovernmentScreen,Globals::InfoScreen,Globals::RangerRatingScreen,
 Globals::RewardsScreen,Globals::ShipScreen,Globals::TalkScreen,Globals::ScannerScreen,Globals::StarMapScreen,Globals::FilmScreen,
 Globals::GalaxyScreen,Globals::JumpScreen,Globals::LoadScreen,Globals::SaveManagerScreen,Globals::GameLoadScreen,
 Globals::GameMenuScreen,Globals::SettingsScreen,Globals::GameEndScreen,Globals::AboutScreen,Globals::ScoreScreen,
 Globals::SelectFaceScreen,Globals::JournalScreen,Globals::LoadRobotScreen,Globals::LoadQuestScreen,Globals::LoadArcadeScreen,Globals::AchievementsScreen};}
void RequireReady(){
 if(owned || aGalaxy::Galaxy || Globals::NewGameGenerationThread || Globals::CacheLoader || aSaveLoad::SaveWriter || Globals::SpaceProcess ||
    Globals::PrimaryFilm || Globals::SecondaryFilm || Globals::SpaceObjectUiLoop || Globals::AchievementsScreen || Globals::PlanetSpaceTemplates.length() ||
    PopUp::PopupController || Globals::PlanetRenderTemplates || GlobalsV::SatelliteRenderTemplates ||
    GlobalsV::SpaceImageTemplates.length() || GlobalsV::StarFieldImageTemplates.length())
  throw std::logic_error("global UI/gameplay runtime already owned or active");
 for(auto* screen:Screens())if(screen)throw std::logic_error("foreign global screen owner");
 aGalaxyStruct::TOwnerId race{};aGalaxyStruct::THullType hull{};aGalaxyStruct::TKlingType kind{};
 for(auto races=pas::for_to<aGalaxyStruct::TOwnerId>(aGalaxyStruct::oiMaloc,aGalaxyStruct::oiPirate);races.next(race);){
  for(auto hulls=pas::for_to<aGalaxyStruct::THullType>(aGalaxyStruct::htRanger,aGalaxyStruct::htDiplomat);hulls.next(hull);)
   if(Globals::RaceShipTemplates[race][hull])throw std::logic_error("foreign race template owner");
  if(Globals::PirateClanShipTemplates[race])throw std::logic_error("foreign pirate template owner");
 }
 for(auto kinds=pas::for_to<aGalaxyStruct::TKlingType>(aGalaxyStruct::ktBoss,aGalaxyStruct::ktKlig);kinds.next(kind);)
  if(kind!=aGalaxyStruct::ktBoss && (Globals::BlazerShipTemplates[kind] || Globals::KellerShipTemplates[kind] || Globals::TerronShipTemplates[kind]))
   throw std::logic_error("foreign dominator template owner");
}
}
struct RangersGlobalUiSession::Impl {
 std::uint8_t reload=Globals::ReloadScriptTemplates;
 decltype(Screens()) screens{};
 decltype(GlobalsV::RegisteredScreens) registered{};
 bool ready{};
 Impl(){
  RequireReady();
  GlobalsV::TGameScreenId id{};
  for(auto range=pas::for_to<GlobalsV::TGameScreenId>(GlobalsV::screenNone,GlobalsV::screenAchievements);range.next(id);)
   if(GlobalsV::RegisteredScreens[id])throw std::logic_error("foreign registered screen owner");
  if(!GR_Main::MainDataConfig || !GR_Main::GlobalCache || !GR_Main::CurrentPixelFormat || !GR_Main::RenderScratchBuffer || !GR_Main::UserSettingsConfig ||
     !Globals::ScriptTemplates || pas::list_count(Globals::ScriptTemplates) || !Globals::GlobalScriptVariables || Globals::GlobalScriptVariables->Count!=2 ||
     !aScript::ScriptFunctionScope || !Globals::TurnCalculationThread || !aScript::ScriptRequestThread || !Globals::ReloadScriptTemplates)
   throw std::logic_error("global UI requires cold real host/script/pixel/scratch/settings owners");
  if(GR_Main::UiStyleConfig!=GR_Main::MainDataConfig->GetBlockByPath(u"ML"_wref.get()) ||
     GR_Main::GameDataConfig!=GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get()) ||
     GR_Main::UiDepthConfig!=GR_Main::MainDataConfig->GetBlockByPath(u"ZPos"_wref.get()))
   throw std::logic_error("global UI requires the original Main.dat roots");
  owned=true;
  try{Globals::InitializeGlobalUiRuntime();screens=Screens();registered=GlobalsV::RegisteredScreens;ready=true;}catch(...){Clear();throw;}
 }
 void Clear()noexcept{
  if(aGalaxy::Galaxy)std::terminate();
  if(ready){
   if(Screens()!=screens)std::terminate();
   GlobalsV::TGameScreenId id{};
   for(auto range=pas::for_to<GlobalsV::TGameScreenId>(GlobalsV::screenNone,GlobalsV::screenAchievements);range.next(id);)
    if(GlobalsV::RegisteredScreens[id]!=registered[id])std::terminate();
  }
  try{
   Globals::FinalizeGlobalUiRuntime();
   // These two original allocations are omitted by FinalizeGlobalUiRuntime.
   pas::free(Globals::AchievementsScreen);Globals::AchievementsScreen=nullptr;
   pas::free(Globals::SpaceObjectUiLoop);Globals::SpaceObjectUiLoop=nullptr;
   Globals::ResetScriptHostRuntimeState();
  }catch(...){std::terminate();}
  Globals::ReloadScriptTemplates=reload;owned=false;
 }
 ~Impl(){Clear();}
};
RangersGlobalUiSession::RangersGlobalUiSession():impl_(std::make_unique<Impl>()){}
RangersGlobalUiSession::~RangersGlobalUiSession()=default;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_campaign_session.hpp"
#include "rangers_achievement_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_Thread.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/MessageText.hpp"
#include "units/WindowsSdk.hpp"
#include "units/aConst.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include "units/aRanger.hpp"
#include "types/aMyFunction.hpp"
#include "types/aEFilm.hpp"
#include "types/fFilmFile.hpp"
#include "types/fGameSettings.hpp"
#include "e2e3_generation_state.hpp"
#include "win32_compat_sync.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
bool campaign_owned{};
void Validate(const RangersCampaignOptions& o){
 if(o.player_name.empty() || o.player_name.find(u'\0')!=o.player_name.npos || o.race>4 || o.character<0 || o.character>5 || o.portrait<0)
  throw std::invalid_argument("invalid original campaign identity");
 for(auto level:o.difficulty)if(level>aConst::MaximumNewGameDifficulty)throw std::invalid_argument("campaign difficulty outside original range");
 for(auto item:o.items)if(item<43 || item>54)throw std::invalid_argument("campaign starting item outside original choices");
 for(auto skill:o.skills)if(skill>5)throw std::invalid_argument("campaign starting skill outside original range");
 if(o.skills[0]==o.skills[1])throw std::invalid_argument("duplicate campaign starting skills");
}
}
struct RangersCampaignSession::Impl {
 std::unique_ptr<RangersAchievementSession> achievements;
 MessageText::TQuestMessages* messages{};
 pas::WideString previous_name=Globals::LastLoadedPlayerName;
 Impl(const RangersCampaignOptions& o){
  Validate(o);
  if(campaign_owned || aGalaxy::Galaxy || aGalaxy::PlayerStar || aPlayer::GetPlayer() || aRanger::PlayerOldQuests || MessageText::QuestMessages || Globals::NewGameGenerationThread || GlobalsV::MemorySnapshotActive)
   throw std::logic_error("campaign already owned or active");
  if(!Globals::GovernmentScreen || !Globals::HangarScreen || !Globals::PrimaryFilm || !Globals::SecondaryFilm || !Globals::SpaceProcess ||
     !GR_Main::CCInterface || !GR_Main::NewGameSettingsConfig || !Globals::ScriptTemplates || !pas::list_count(Globals::ScriptTemplates) ||
     !Globals::TurnCalculationThread || !Globals::GlobalScriptVariables || Globals::ReloadScriptTemplates || GR_Main::ExitScreenLoop)
   throw std::logic_error("campaign requires the complete original UI/gameplay/script/TCC context");
  campaign_owned=true;
  try{
   achievements=std::make_unique<RangersAchievementSession>();
   // Original GR_Main platform startup constructs this service before quests.
   // Its real constructor owns Entries; all lookups retain original semantics.
   messages=pas::construct_call<MessageText::TQuestMessages>(MessageText::TQuestMessages_Create);MessageText::QuestMessages=messages;
   Globals::NewGameGenerationThread=pas::construct_call<fGameSettings::TThreadCreateNewGame>(EC_Thread::TThreadEC_Create);
   auto* thread=Globals::NewGameGenerationThread;
   thread->PlayerName=pas::WideString(std::u16string_view(o.player_name));thread->PlayerRace=static_cast<aGalaxyStruct::TOwnerId>(o.race);
   thread->CharacterPreset=o.character;thread->CaptainPortraitIndex=o.portrait;thread->IronWill=o.iron_will;
   for(int i=0;i<8;++i)thread->DifficultyLevels[i]=o.difficulty[i];
   for(int i=0;i<2;++i){thread->StartingItemTypes[i]=o.items[i];thread->StartingSkills[i]=static_cast<aGalaxyStruct::TPilotSkill>(o.skills[i]);}
   Globals::FilmHistory->Clear();thread->SetPriority(2);
   srhd_awa::platform::e2e3_generation_state::Reset();thread->Start();
   for(;;){
    auto wait=WindowsSdk::WaitForSingleObject(thread->IdleEvent,1000);
    if(wait==WindowsSdk::WAIT_OBJECT_0)break;
    if(wait!=WindowsSdk::WAIT_TIMEOUT || !srhd_awa::platform::win32_compat::IsThreadUsable(thread->ThreadHandle))throw std::runtime_error("original generation worker failed");
   }
   if(srhd_awa::platform::e2e3_generation_state::Failed() || srhd_awa::platform::e2e3_generation_state::Stage()!=8 ||
      !aGalaxy::Galaxy || !aPlayer::GetPlayer() || !aPlayer::GetPlayer()->CurrentStar || !aPlayer::GetPlayer()->HomePlanet || GR_Main::ExitScreenLoop)
    throw std::runtime_error("original campaign generator did not finish");
  }catch(...){Clear();throw;}
 }
 void Clear()noexcept{
  try{
   pas::free(Globals::NewGameGenerationThread);Globals::NewGameGenerationThread=nullptr;
   pas::free(aGalaxy::Galaxy);aGalaxy::Galaxy=nullptr;
   pas::free(aRanger::PlayerOldQuests);aRanger::PlayerOldQuests=nullptr;
   pas::free(messages);messages=nullptr;MessageText::QuestMessages=nullptr;
   Globals::LastLoadedPlayerName=previous_name;
   achievements.reset();campaign_owned=false;
  }catch(...){std::terminate();}
 }
 // Original memory snapshots destroy/rebuild Galaxy. Own the exclusive global
 // campaign lifetime rather than retaining a pointer to the destroyed world.
 ~Impl(){if(!campaign_owned || GlobalsV::MemorySnapshotActive || MessageText::QuestMessages!=messages)std::terminate();Clear();}
};
RangersCampaignSession::RangersCampaignSession(const RangersCampaignOptions& o):impl_(std::make_unique<Impl>(o)){}
RangersCampaignSession::~RangersCampaignSession()=default;
aGalaxy::TGalaxy& RangersCampaignSession::Galaxy()const{
 if(GlobalsV::MemorySnapshotActive || !aGalaxy::Galaxy)throw std::logic_error("campaign world detached or absent");
 return *aGalaxy::Galaxy;
}
}

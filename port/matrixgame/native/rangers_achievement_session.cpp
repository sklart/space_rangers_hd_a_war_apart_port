// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_achievement_session.hpp"
#include "runtime_support.hpp"
#include "units/Achievements.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/NoSteamAchievemens.hpp"
#include "units/aGalaxy.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
struct RangersAchievementSession::Impl {
 EC_BlockPar::TBlockParEC* definitions{};
 Impl(){
  if(!GR_Main::MainDataConfig || !GR_Main::GlobalCache || aGalaxy::Galaxy || Achievements::AchievementDefinitions)
   throw std::logic_error("achievements require a cold real data host and unowned definitions");
  if(Achievements::GetAchievementBackend()!=Achievements::achLocal)
   throw std::logic_error("standalone achievement owner requires the original local backend");
  try{
   Achievements::InitializeAchievementDefinitions();definitions=Achievements::AchievementDefinitions;
   if(!definitions || definitions->GetBlockCount()!=82)throw std::runtime_error("original achievement definitions incomplete");
   NoSteamAchievemens::LoadLocalAchievements();
  }catch(...){Clear();throw;}
 }
 void Clear()noexcept{
  if(aGalaxy::Galaxy)std::terminate();
  if(definitions && Achievements::AchievementDefinitions!=definitions)std::terminate();
  try{pas::free(Achievements::AchievementDefinitions);Achievements::AchievementDefinitions=nullptr;definitions=nullptr;}catch(...){std::terminate();}
 }
 ~Impl(){Clear();}
};
RangersAchievementSession::RangersAchievementSession():impl_(std::make_unique<Impl>()){}
RangersAchievementSession::~RangersAchievementSession()=default;
}

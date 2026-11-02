#ifdef NDEBUG
#error Firing verification requires assertions
#endif
#include "matrixgame_firing_probe.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixRobot.hpp"
#include "MatrixSide.hpp"
#include "MatrixConfig.hpp"
#include "native_input_engine.hpp"
#include "input.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
namespace {FiringProbe* current{};}
FiringProbe::FiringProbe(){assert(!current);current=this;}
FiringProbe::~FiringProbe(){assert(current==this);current=nullptr;if(fire_key_)srhd_awa::matrixgame::native_input::EngineSink{}.Key(false,fire_key_);if(core_){if(g_MatrixMap && core_->m_Object && g_MatrixMap->GetPlayerSide()->GetArcadedObject()==core_->m_Object)g_MatrixMap->GetPlayerSide()->SetArcadedObject(nullptr);core_->Release();}}
void FiringProbe::Issue(){
 assert(!core_ && g_MatrixMap && !g_MatrixMap->IsPaused());CMatrixRobotAI* robot{};float range{};
 for(auto* object=CMatrixMapStatic::GetFirstLogic();object;object=object->GetNextLogic())if(object->IsLiveRobot() && object->GetSide()==PLAYER_SIDE && !object->AsRobot()->GetBase()){
  auto* candidate=object->AsRobot();bool safe=true;float reach{};
  for(int i=0;i<MAX_WEAPON_CNT;++i){const auto& weapon=candidate->GetWeapon(i);if(!weapon.IsEffectPresent())continue;const auto type=weapon.GetWeaponType();if(type==WEAPON_BOMB || type==WEAPON_BIGBOOM)safe=false;else if(type!=WEAPON_REPAIR)reach=weapon.GetWeaponDist();}
  const auto& forward=candidate->m_HullForward;if(safe && reach>40 && forward.x*forward.x+forward.y*forward.y>.25f){robot=candidate;range=reach;break;}
 }
 assert(robot);core_=robot->GetCore(DEBUG_CALL_INFO);start_=g_MatrixMap->GetTime();
 auto* side=g_MatrixMap->GetPlayerSide();side->CreateGroupFromCurrent(robot);assert(side->GetCurGroup());side->Select(ROBOT,robot);
 side->SetArcadedObject(robot);assert(side->GetArcadedObject()==robot);
 fire_key_=g_Config.m_KeyActions[KA_FIRE];assert(fire_key_);srhd_awa::matrixgame::native_input::EngineSink{}.Key(true,fire_key_);assert(Input::isKeyPressed(KA_FIRE));
 std::printf("FIRING_COMMAND time=%d range=%.6f key=%u original_arcade_EngineSink_KA_FIRE=EXECUTED\n",start_,range,unsigned(fire_key_));
}
void FiringProbe::Stop(){assert(core_ && core_->m_Object && core_->m_Object->IsLiveRobot());stop_=g_MatrixMap->GetTime();srhd_awa::matrixgame::native_input::EngineSink{}.Key(false,fire_key_);assert(!Input::isKeyPressed(KA_FIRE));std::printf("FIRING_STOP time=%d held_ms=%d original_EngineSink_key_release=EXECUTED\n",stop_,stop_-start_);}
void ObserveOriginalFire(CMatrixEffectWeapon& weapon,SObjectCore* owner){
 if(!current || !current->core_ || owner!=current->core_)return;
 auto& probe=*current;auto& count=probe.weapons_[&weapon];const int actual=weapon.GetFireCount();
 if(!count.events){count.first=actual;count.type=int(weapon.GetWeaponType());}else assert(actual==count.last+1);
 count.last=actual;++count.events;++probe.shots_;probe.last_fire_=g_MatrixMap->GetTime();
}
void ObserveOriginalHit(CMatrixEffectWeapon&,SObjectCore* owner,CMatrixMapStatic* hit,std::uint32_t){
 if(!current || !current->core_ || owner!=current->core_)return;
 ++current->hits_;if(IS_TRACE_STOP_OBJECT(hit))++current->object_hits_;if(hit==TRACE_STOP_LANDSCAPE)++current->land_hits_;
}
void FiringProbe::Verify(){
 assert(core_ && core_->m_Object && core_->m_Object->IsLiveRobot());assert(g_MatrixMap->GetTime()-start_==5000 && stop_-start_==4000);
 std::fprintf(stderr,"FIRING_DIAGNOSTIC shots=%u hits=%u weapons=%zu orders=%d last_fire=%d start=%d stop=%d\n",shots_,hits_,weapons_.size(),core_->m_Object->AsRobot()->GetOrdersInPool(),last_fire_,start_,stop_);
 assert(shots_>0 && hits_>0 && !weapons_.empty());unsigned total{};
 for(const auto& [weapon,count]:weapons_){(void)weapon;assert(count.last-count.first+1==count.events);total+=unsigned(count.events);std::printf("FIRING_WEAPON type=%d first=%d last=%d events=%d\n",count.type,count.first,count.last,count.events);}
 assert(total==shots_ && last_fire_<=stop_+500);
 std::printf("FIRING_RESULT elapsed=5000 held=4000 settle=1000 weapons=%zu shots=%u hit_callbacks=%u object_hits=%u landscape_hits=%u last_fire=%d stop=%d original_callbacks=PASS counter_consistency=PASS stop_effective=PASS live_robot=PASS\n",weapons_.size(),shots_,hits_,object_hits_,land_hits_,last_fire_,stop_);
}

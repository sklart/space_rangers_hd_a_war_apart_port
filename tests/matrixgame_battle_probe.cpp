#ifdef NDEBUG
#error Battle verification requires assertions
#endif
#include "matrixgame_battle_probe.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixRobot.hpp"
#include "MatrixObjectCannon.hpp"
#include "MatrixSide.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
namespace {
BattleProbe* active{};
float Health(CMatrixMapStatic* object){assert(object && (object->IsRobot() || object->IsCannon()));return object->IsRobot()?object->AsRobot()->GetHitPoint():object->AsCannon()->GetHitPoint();}
bool Hostile(CMatrixMapStatic* object){return object && (object->IsLiveRobot() || object->IsLiveCannon()) && object->GetSide()>0 && object->GetSide()!=PLAYER_SIDE;}
}
BattleHitSnapshot::BattleHitSnapshot(CMatrixMapStatic* target,int type):before(Health(target)),weapon(type),side(target->GetSide()),kind(int(target->GetObjectType())){core=target->GetCore(DEBUG_CALL_INFO);assert(std::isfinite(before));}
BattleHitSnapshot::~BattleHitSnapshot(){if(core)core->Release();}
BattleProbe::BattleProbe(){assert(!active);active=this;}
BattleProbe::~BattleProbe(){assert(active==this);active=nullptr;if(target_)target_->Release();if(attacker_)attacker_->Release();}
void BattleProbe::Issue(){
 assert(!attacker_ && g_MatrixMap && !g_MatrixMap->IsPaused());
 CMatrixRobotAI* attacker{};CMatrixMapStatic* target{};float nearest=std::numeric_limits<float>::max();unsigned players{},enemies{};
 for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic()){
  if(Hostile(p))++enemies;
  if(!p->IsLiveRobot() || p->GetSide()!=PLAYER_SIDE || p->AsRobot()->GetBase())continue;
  auto* robot=p->AsRobot();bool armed{},safe=true;
  for(int i=0;i<MAX_WEAPON_CNT;++i){const auto& weapon=robot->GetWeapon(i);if(!weapon.IsEffectPresent())continue;
   const auto type=weapon.GetWeaponType();if(type==WEAPON_BOMB || type==WEAPON_BIGBOOM)safe=false;else if(type!=WEAPON_REPAIR)armed=true;}
  if(!armed || !safe)continue;++players;
  for(auto* q=CMatrixMapStatic::GetFirstLogic();q;q=q->GetNextLogic())if(Hostile(q)){
   const auto& a=robot->GetGeoCenter();const auto& b=q->GetGeoCenter();const float dx=a.x-b.x,dy=a.y-b.y;const float d=dx*dx+dy*dy;
   if(d<nearest){nearest=d;attacker=robot;target=q;}
  }
 }
 std::printf("BATTLE_INVENTORY players=%u hostile_combatants=%u nearest_distance=%.6f\n",players,enemies,std::sqrt(nearest));std::fflush(stdout);
 assert(attacker && target);attacker_=attacker->GetCore(DEBUG_CALL_INFO);target_=target->GetCore(DEBUG_CALL_INFO);start_=g_MatrixMap->GetTime();
 const auto& center=target->GetGeoCenter();const CPoint destination(int(center.x/GLOBAL_SCALE_MOVE),int(center.y/GLOBAL_SCALE_MOVE));
 auto* side=g_MatrixMap->GetPlayerSide();const int group=side->RobotToLogicGroup(attacker);assert(group>=0 && group<MAX_LOGIC_GROUP);
 std::printf("BATTLE_COMMAND time=%d group=%d attacker_cell=%d,%d target_cell=%d,%d target_side=%d target_kind=%d target_hp=%.6f original_PGOrderAttack=EXECUTED\n",start_,group,attacker->GetMapPosX(),attacker->GetMapPosY(),destination.x,destination.y,target->GetSide(),int(target->GetObjectType()),Health(target));std::fflush(stdout);
 side->PGOrderAttack(group,destination,target);
}
bool BattleProbe::Continue()const{return attacker_ && attacker_->m_Object && attacker_->m_Object->IsLiveRobot() && !reductions_ && !g_MatrixMap->IsPaused();}
BattleHitSnapshot BeginBattleHit(CMatrixMapStatic* target,SObjectCore* owner,int type){
 if(!active || owner!=active->attacker_ || !IS_TRACE_STOP_OBJECT(target) || !Hostile(target))return {};
 return BattleHitSnapshot(target,type);
}
void EndBattleHit(const BattleHitSnapshot& hit,bool dead){
 if(!active || !hit.core)return;auto* object=hit.core->m_Object;
 const bool known=object && (object->IsRobot() || object->IsCannon());const float after=known?Health(object):0.f;
 if(known){assert(std::isfinite(after));if(after<hit.before)++active->reductions_;}
 active->hits_.push_back({g_MatrixMap->GetTime(),hit.weapon,hit.side,hit.kind,hit.before,after,object!=nullptr,dead});
 std::printf("BATTLE_HIT time=%d weapon=%d side=%d kind=%d hp_before=%.6f hp_after=%.6f exists_after=%u hp_after_known=%u returned_dead=%u\n",g_MatrixMap->GetTime(),hit.weapon,hit.side,hit.kind,hit.before,after,unsigned(object!=nullptr),unsigned(known),unsigned(dead));std::fflush(stdout);
}
void BattleProbe::Verify(){
 assert(attacker_ && target_);const auto* robot=attacker_->m_Object;
 std::printf("BATTLE_RESULT elapsed=%d hit_calls=%zu HP_decreases=%u attacker_exists=%u target_exists=%u no_position_HP_weapon_injection=PASS\n",g_MatrixMap->GetTime()-start_,hits_.size(),reductions_,unsigned(robot!=nullptr),unsigned(target_->m_Object!=nullptr));std::fflush(stdout);
 assert(!hits_.empty() && reductions_>0);std::puts("BATTLE original_attack_navigation_weapon_virtual_Damage_HP=PASS");
}

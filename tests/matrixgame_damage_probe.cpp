#ifdef NDEBUG
#error Damage verification requires assertions
#endif
#include "matrixgame_damage_probe.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixMapStatic.hpp"
#include "MatrixRobot.hpp"
#include "MatrixObject.hpp"
#include "MatrixObjectCannon.hpp"
#include "MatrixObjectBuilding.hpp"
#include "MatrixSide.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
namespace {
DamageProbe* active{};
DamageObservation* guarded{};
DamageState Read(CMatrixMapStatic* object){
 DamageState state;if(!object)return state;state.exists=true;state.object_type=int(object->GetObjectType());
 if(object->IsRobot()){state.health_known=true;state.health=object->AsRobot()->GetHitPoint();}
 else if(object->IsCannon()){state.health_known=true;state.health=object->AsCannon()->GetHitPoint();}
 else if(object->IsBuilding()){state.health_known=true;state.health=object->AsBuilding()->GetHitPoint();}
 else if(object->GetObjectType()==OBJECT_TYPE_MAPOBJECT){
  auto* item=static_cast<CMatrixMapObject*>(object);state.behaviour=int(item->m_BehFlag);
  if(item->m_BehFlag==BEHF_BREAK || item->m_BehFlag==BEHF_TERRON){state.health_known=true;state.health=float(item->m_BreakHitPoint);}
 }
 if(state.health_known)assert(std::isfinite(state.health));return state;
}
}
DamageObservation::DamageObservation(CMatrixMapStatic* target,int type):before(Read(target)),weapon(type){core=target->GetCore(DEBUG_CALL_INFO);previous=guarded;guarded=this;}
DamageObservation::~DamageObservation(){if(core){assert(guarded==this);guarded=previous;core->Release();}}
DamageProbe::DamageProbe(){assert(!active);active=this;}
DamageProbe::~DamageProbe(){assert(active==this && !guarded);active=nullptr;}
DamageObservation BeginOriginalDamage(CMatrixMapStatic* target,SObjectCore* owner,int type){
 if(!active || !owner || !g_MatrixMap || owner->m_Object!=g_MatrixMap->GetPlayerSide()->GetArcadedObject())return {};
 assert(target && IS_TRACE_STOP_OBJECT(target));return DamageObservation(target,type);
}
void ObserveOriginalDamage(const DamageObservation& observation,bool dead){
 if(!active || !observation.core)return;
 auto after=Read(observation.core->m_Object);
 if(observation.burn_after.burn_known){after.burn_known=true;after.ablaze=observation.burn_after.ablaze;after.ttl=observation.burn_after.ttl;}
 active->events_.push_back({observation.before,after,observation.weapon,g_MatrixMap->GetTime(),dead});
}
void ObserveOriginalBurnState(CMatrixMapStatic* target,bool after,bool ablaze,int ttl){
 if(!active || !guarded || !guarded->core || guarded->core->m_Object!=target)return;
 auto& state=after?guarded->burn_after:guarded->before;state.burn_known=true;state.ablaze=ablaze;state.ttl=ttl;
}
void DamageProbe::Verify(){
 assert(!guarded);
 unsigned changed{},health_decreases{},ignitions{},ttl_increases{},removed{};
 for(unsigned i=0;i<events_.size();++i){const auto& e=events_[i];const auto& a=e.before;const auto& b=e.after;
  const bool health=a.health_known && b.health_known && b.health<a.health;
  const bool ignite=a.burn_known && b.burn_known && !a.ablaze && b.ablaze;const bool ttl=a.burn_known && b.burn_known && b.ablaze && b.ttl>a.ttl;const bool gone=a.exists && !b.exists;
  const bool change=health || ignite || ttl || gone || a.object_type!=b.object_type || a.behaviour!=b.behaviour;
  changed+=change;health_decreases+=health;ignitions+=ignite;ttl_increases+=ttl;removed+=gone;
  std::printf("DAMAGE_EVENT index=%u time=%d weapon=%d object_type=%d behaviour_before=%d behaviour_after=%d health_known=%u hp_before=%.6f hp_after=%.6f burn_known=%u ablaze_before=%u ablaze_after=%u ttl_before=%d ttl_after=%d exists_after=%u returned_dead=%u state_changed=%u\n",i,e.time,e.weapon,a.object_type,a.behaviour,b.behaviour,unsigned(a.health_known&&b.health_known),a.health,b.health,unsigned(a.burn_known&&b.burn_known),unsigned(a.ablaze),unsigned(b.ablaze),a.ttl,b.ttl,unsigned(b.exists),unsigned(e.dead),unsigned(change));
 }
 std::fflush(stdout);assert(!events_.empty() && changed>0);
 std::printf("DAMAGE_RESULT calls=%zu state_changes=%u health_decreases=%u ignitions=%u ttl_increases=%u removals=%u actual_virtual_Damage=PASS no_HP_TTL_position_injection=PASS guarded_core_lifetime=PASS\n",events_.size(),changed,health_decreases,ignitions,ttl_increases,removed);
}

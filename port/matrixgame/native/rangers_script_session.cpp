// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_script_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_Expression.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPath.hpp"
#include "units/aSaveLoad.hpp"
#include "units/aScript.hpp"
#include "types/aMyFunction.hpp"
#include "types/ThreadCalc.hpp"
#include "script_array_lifetime.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
void RequireUnbound(){
 if(!GR_Main::MainDataConfig || !GR_Main::GlobalCache)throw std::logic_error("Rangers scripts require a live data/cache host");
 if(aGalaxy::Galaxy || Globals::PersistentPlayerMessageLock || aSaveLoad::SaveLoadLock || Globals::FilmHistory ||
    Globals::TalkRequestEvent || Globals::TalkCompletedEvent || Globals::ScriptUiRequestEvent || Globals::ScriptUiAbortEvent ||
    Globals::TurnCalculationThread || Globals::ScriptTemplates || Globals::GlobalScriptVariables || Globals::SharedScriptVariables ||
    aScript::ScriptProcess || aScript::ScriptFunctionScope || aScript::CurrentScript || aScript::ScriptRequestThread ||
    aScript::QueuedTextQuests || aScript::QueuedArcadeBattles || aScript::QueuedPlanetaryBattles || aScript::QueuedVideos || aScript::StagedArcadeShips ||
    Globals::ScriptItemContextStack || Globals::ScriptItemInfoContextStack || Globals::ScriptActionShipStack ||
    Globals::ScriptActionObject1Stack || Globals::ScriptActionObject2Stack || Globals::ScriptActionParamStack || Globals::ScriptActionTypeStack ||
    aPath::PathNodeHeap || aPath::PathPoolLock || aPath::PathInitialBlock || aPath::PathPoolHead || aPath::PathPoolTail || aPath::PathGrowthBlocks.length())
  throw std::logic_error("Rangers script/path runtime already owned or active");
 aGalaxyStruct::TOwnerId race{};aGalaxyStruct::THullType hull{};aGalaxyStruct::TKlingType kind{};
 for(auto range=pas::for_to<aGalaxyStruct::TOwnerId>(aGalaxyStruct::oiMaloc,aGalaxyStruct::oiPirate);range.next(race);){
  for(auto hulls=pas::for_to<aGalaxyStruct::THullType>(aGalaxyStruct::htRanger,aGalaxyStruct::htDiplomat);hulls.next(hull);)
   if(Globals::RaceShipTemplates[race][hull])throw std::logic_error("foreign race ship template owner");
  if(Globals::PirateClanShipTemplates[race])throw std::logic_error("foreign pirate ship template owner");
 }
 // FinalizeScriptHostRuntime releases these too; refuse unrelated owners.
 for(auto kinds=pas::for_to<aGalaxyStruct::TKlingType>(aGalaxyStruct::ktBoss,aGalaxyStruct::ktKlig);kinds.next(kind);)
  if(kind!=aGalaxyStruct::ktBoss && (Globals::BlazerShipTemplates[kind] || Globals::KellerShipTemplates[kind] || Globals::TerronShipTemplates[kind]))
   throw std::logic_error("foreign dominator ship template owner");
}
}
struct RangersScriptSession::Impl {
 srhd_awa::platform::script_arrays::Scope arrays;
 EC_Expression::TScriptStepCallback callback=EC_Expression::ScriptStepCallback;
 int interval=EC_Expression::ScriptStepInterval;
 decltype(EC_Expression::ScriptCallTrace) trace=EC_Expression::ScriptCallTrace;
 int trace_position=EC_Expression::ScriptCallTracePosition,trace_count=EC_Expression::ScriptCallTraceCount;
 int pool_count=aPath::PathPoolFreeCount,growth_count=aPath::PathGrowthBlockCount;
 Impl(){
  RequireUnbound();
  try{
   Globals::InitializeScriptHostRuntime();
   if(!Globals::TalkRequestEvent || !Globals::TalkCompletedEvent || !Globals::ScriptUiRequestEvent || !Globals::ScriptUiAbortEvent ||
      !Globals::TurnCalculationThread || !Globals::TurnCalculationThread->ThreadHandle || !aPath::PathInitialBlock || !aPath::PathNodeHeap ||
      !aScript::ScriptProcess || !aScript::ScriptFunctionScope || !Globals::GlobalScriptVariables || !Globals::SharedScriptVariables ||
      !aScript::ScriptRequestThread || !aScript::ScriptRequestThread->ThreadHandle || !aScript::StagedArcadeShips ||
      !aScript::QueuedTextQuests || !aScript::QueuedArcadeBattles || !aScript::QueuedPlanetaryBattles || !aScript::QueuedVideos ||
      !Globals::ScriptItemContextStack || !Globals::ScriptItemInfoContextStack || !Globals::ScriptActionShipStack ||
      !Globals::ScriptActionObject1Stack || !Globals::ScriptActionObject2Stack || !Globals::ScriptActionParamStack || !Globals::ScriptActionTypeStack)
    throw std::runtime_error("original Rangers script runtime did not acquire mandatory resources");
  }catch(...){Clear();throw;}
 }
 void Clear()noexcept{
  if(aGalaxy::Galaxy || aScript::CurrentScript)std::terminate();
  try{
   // Builtin registration also owns a request worker and twelve containers.
   // Original FinalizeScriptEngine only releases the VM and function scope.
   // Join the real worker before releasing its queues, VM, or host events.
   pas::free(aScript::ScriptRequestThread);aScript::ScriptRequestThread=nullptr;
   if(aScript::QueuedTextQuests && aScript::QueuedArcadeBattles && aScript::QueuedPlanetaryBattles && aScript::QueuedVideos && aScript::StagedArcadeShips)
    aScript::ClearPendingScriptRequests();
   pas::free(aScript::StagedArcadeShips);aScript::StagedArcadeShips=nullptr;
   for(auto* queue:{&aScript::QueuedTextQuests,&aScript::QueuedArcadeBattles,&aScript::QueuedPlanetaryBattles,&aScript::QueuedVideos,
       &Globals::ScriptItemContextStack,&Globals::ScriptItemInfoContextStack,&Globals::ScriptActionShipStack,
       &Globals::ScriptActionObject1Stack,&Globals::ScriptActionObject2Stack,&Globals::ScriptActionParamStack,&Globals::ScriptActionTypeStack}){
    pas::free(*queue);*queue=nullptr;
   }
   Globals::FinalizeScriptHostRuntime();
   arrays.End();
  }catch(...){std::terminate();}
  // Original finalization frees the pool but leaves its list heads/counters.
  // No consumers survive this owner; restore the previously unbound state.
  aPath::PathPoolHead=nullptr;aPath::PathPoolTail=nullptr;
  aPath::PathPoolFreeCount=pool_count;aPath::PathGrowthBlockCount=growth_count;
  EC_Expression::SetScriptStepCallback(callback,interval);
  EC_Expression::ScriptCallTrace=trace;EC_Expression::ScriptCallTracePosition=trace_position;EC_Expression::ScriptCallTraceCount=trace_count;
 }
 ~Impl(){Clear();}
};
RangersScriptSession::RangersScriptSession():impl_(std::make_unique<Impl>()){}
RangersScriptSession::~RangersScriptSession()=default;
}

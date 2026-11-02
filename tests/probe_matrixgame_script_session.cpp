// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#error Script lifecycle evidence requires assertions
#endif
#include "rangers_script_session.hpp"
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "units/EC_Expression.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/WindowsSdk.hpp"
#include "units/WindowsImports.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPath.hpp"
#include "units/aSaveLoad.hpp"
#include "units/aScript.hpp"
#include "types/ThreadCalc.hpp"
#include "win32_handles.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace m=srhd_awa::matrixgame;
namespace {
int observed{};
void Observe(int value){observed=value;}
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(const std::logic_error&){rejected=true;}assert(rejected);}
void Empty(){
 assert(!Globals::PersistentPlayerMessageLock && !aSaveLoad::SaveLoadLock && !Globals::FilmHistory && !Globals::TurnCalculationThread);
 assert(!Globals::TalkRequestEvent && !Globals::TalkCompletedEvent && !Globals::ScriptUiRequestEvent && !Globals::ScriptUiAbortEvent);
 assert(!Globals::ScriptTemplates && !Globals::GlobalScriptVariables && !Globals::SharedScriptVariables && !aScript::ScriptProcess && !aScript::ScriptFunctionScope);
 assert(!aScript::ScriptRequestThread && !aScript::QueuedTextQuests && !aScript::QueuedArcadeBattles && !aScript::QueuedPlanetaryBattles && !aScript::QueuedVideos && !aScript::StagedArcadeShips);
 assert(!Globals::ScriptItemContextStack && !Globals::ScriptItemInfoContextStack && !Globals::ScriptActionShipStack && !Globals::ScriptActionObject1Stack && !Globals::ScriptActionObject2Stack && !Globals::ScriptActionParamStack && !Globals::ScriptActionTypeStack);
 assert(!aPath::PathNodeHeap && !aPath::PathPoolLock && !aPath::PathInitialBlock && !aPath::PathPoolHead && !aPath::PathPoolTail && !aPath::PathGrowthBlocks.length());
}
void Events(){
 assert(WindowsSdk::WaitForSingleObject(Globals::TalkRequestEvent,0)==WindowsSdk::WAIT_TIMEOUT);
 assert(WindowsSdk::SetEvent(Globals::TalkRequestEvent));
 assert(WindowsSdk::WaitForSingleObject(Globals::TalkRequestEvent,0)==WindowsSdk::WAIT_OBJECT_0);
 assert(WindowsSdk::WaitForSingleObject(Globals::TalkRequestEvent,0)==WindowsSdk::WAIT_TIMEOUT);
 assert(WindowsSdk::SetEvent(Globals::ScriptUiRequestEvent));
 assert(WindowsSdk::WaitForSingleObject(Globals::ScriptUiRequestEvent,0)==WindowsSdk::WAIT_OBJECT_0);
 assert(WindowsSdk::WaitForSingleObject(Globals::ScriptUiRequestEvent,0)==WindowsSdk::WAIT_OBJECT_0);
 assert(WindowsSdk::ResetEvent(Globals::ScriptUiRequestEvent));
 assert(WindowsSdk::WaitForSingleObject(Globals::ScriptUiRequestEvent,0)==WindowsSdk::WAIT_TIMEOUT);
}
}
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Empty();Reject([]{m::RangersScriptSession missing;});Empty();
 const auto previous_callback=EC_Expression::ScriptStepCallback;const auto previous_interval=EC_Expression::ScriptStepInterval;
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);
  const auto foreign=WindowsImports::CreateEvent(nullptr,0,0,nullptr);assert(foreign);Globals::TalkRequestEvent=foreign;
  Reject([]{m::RangersScriptSession duplicate;});assert(Globals::TalkRequestEvent==foreign && WindowsSdk::SetEvent(foreign));
  assert(WindowsImports::CloseHandle(foreign));Globals::TalkRequestEvent=0;Empty();
  Globals::ScriptItemContextStack=pas::make_object<pas::List>();auto* foreign_stack=Globals::ScriptItemContextStack;
  Reject([]{m::RangersScriptSession duplicate;});assert(Globals::ScriptItemContextStack==foreign_stack);
  pas::free(foreign_stack);Globals::ScriptItemContextStack=nullptr;Empty();
  for(int cycle=0;cycle<2;++cycle){
   EC_Expression::SetScriptStepCallback(EC_Expression::TScriptStepCallback(&Observe),17);
   const int free_before=aPath::PathPoolFreeCount,growth_before=aPath::PathGrowthBlockCount;
   std::array<std::uint32_t,14> events{};std::array<std::uint32_t,2> threads{};
   {
    m::RangersScriptSession scripts;assert(aPath::PathPoolFreeCount==200000 && aPath::PathInitialBlock && aPath::PathPoolHead && aPath::PathPoolTail);
    assert(Globals::GlobalScriptVariables->GetVar(u"GRunFrom"_wref.get())->GetInt()==0);
    assert(Globals::GlobalScriptVariables->GetVar(u"GRunStar"_wref.get())->Kind==EC_Expression::vkDword);
    const int functions=aScript::ScriptFunctionScope->Count;assert(functions>100);
    assert(!Globals::TurnCalculationThread->IsRunning());
    assert(aScript::ScriptRequestThread && !aScript::ScriptRequestThread->IsRunning());
    assert(WindowsSdk::WaitForSingleObject(aScript::ScriptRequestThread->IdleEvent,0)==WindowsSdk::WAIT_OBJECT_0);
    assert(WindowsSdk::WaitForSingleObject(Globals::TurnCalculationThread->IdleEvent,0)==WindowsSdk::WAIT_OBJECT_0);
    Reject([]{m::RangersScriptSession duplicate;});
    Globals::SharedScriptVariables->CopyFrom(Globals::GlobalScriptVariables,true);
    aScript::ExecuteScriptText(u"GRunFrom = abs(-7) + 35;"_w,nullptr);
    assert(Globals::SharedScriptVariables->GetVar(u"GRunFrom"_wref.get())->GetInt()==42);
    auto* path=pas::construct_call<aPath::TSPath>(aPath::TSPath_Create);assert(aPath::PathPoolFreeCount==200000-24);
    path->AppendWaypoint({1.f,2.f},3.f);path->AppendWaypoint({5.f,6.f},7.f);assert(path->NodeCount==2);
    assert(path->ActiveHead->Position.X==1.f && path->ActiveTail->Position.Y==6.f);
    pas::free(path);assert(aPath::PathPoolFreeCount==200000);Events();
    auto* calculation=Globals::TurnCalculationThread;auto* requests=aScript::ScriptRequestThread;
    events={Globals::TalkRequestEvent,Globals::TalkCompletedEvent,Globals::ScriptUiRequestEvent,Globals::ScriptUiAbortEvent,
     calculation->StopEvent,calculation->ShutdownEvent,calculation->StartEvent,calculation->RunningEvent,calculation->IdleEvent,
     requests->StopEvent,requests->ShutdownEvent,requests->StartEvent,requests->RunningEvent,requests->IdleEvent};
    threads={calculation->ThreadHandle,requests->ThreadHandle};
    assert(!aGalaxy::Galaxy);
    std::printf("SCRIPT_SESSION cycle=%d builtins=%d original_VM_abs_assignment=42 path_allocate_recycle=PASS auto_manual_events=PASS real_worker_standby=PASS Galaxy=NOT_CREATED\n",cycle,functions);
   }
   Empty();assert(aPath::PathPoolFreeCount==free_before && aPath::PathGrowthBlockCount==growth_before);
   assert(EC_Expression::ScriptStepInterval==17 && pas::assigned(EC_Expression::ScriptStepCallback));EC_Expression::ScriptStepCallback(123+cycle);assert(observed==123+cycle);
   for(auto handle:events)assert(!srhd_awa::platform::win32_compat::Handles().IsValid(handle,srhd_awa::platform::win32_compat::HandleType::Event));
   for(auto handle:threads)assert(!srhd_awa::platform::win32_compat::Handles().IsValid(handle,srhd_awa::platform::win32_compat::HandleType::Thread));
   std::printf("SCRIPT_SESSION cycle=%d both_workers_joined=PASS all_14_events_closed=PASS builtin_containers_released=PASS foreign_stack=REJECTED\n",cycle);
  }
  EC_Expression::SetScriptStepCallback(previous_callback,previous_interval);
 }
 Empty();assert(!m::EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::GlobalCache);
 std::puts("SCRIPT_SESSION original_Finalize=PASS events_worker_closed=PASS pool_heads_counts_callback_restored=PASS foreign_owner=REJECTED no_save_load=PASS NO_WORLD_GENERATION");
}

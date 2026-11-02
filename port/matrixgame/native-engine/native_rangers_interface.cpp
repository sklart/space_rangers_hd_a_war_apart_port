// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_rangers_interface.hpp"
#include "native_rangers_support.hpp"
#include "native_rangers_services.hpp"
#include "engine_callbacks.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "units/Robot.hpp"
#include <stdexcept>
#ifdef __SWITCH__
#include "native_rangers_canvas.hpp"
#include "platform_switch.hpp"
#include "rangers_caller_presentation.hpp"
#endif
namespace srhd_awa::matrixgame {
static_assert(SUPE_OK==0 && SUPE_DIRECTX==1 && SUPE_VIDEODRIVER==2 && SUPE_VIDEOHARDWARE==3);
struct NativeRangersInterface::State {
 platform3g::Device& device;
#ifdef __SWITCH__
 // Outlives Run/callback owners; removed before the borrowed device dies.
 std::unique_ptr<RangersCallerPresentation> caller;
 std::unique_ptr<NativeRangersCanvas> canvas;
#endif
 std::unique_ptr<NativeRangersRunBinding> run;
 std::unique_ptr<EngineCallbacks> callbacks;
 std::unique_ptr<NativeRangersServicesScope> services;
 Robot::TRobotInterfacePrefix table{};bool running{};
 explicit State(const NativeGameOptions& input):device(input.device){
  if(input.services)throw std::invalid_argument("native Robot interface initializes its own copied callback table");
#ifdef __SWITCH__
  auto* deko=dynamic_cast<platform3g::DekoDevice*>(&device);
  if(!deko)throw std::invalid_argument("Switch Robot interface requires the real deko canvas provider");
  auto* window=dynamic_cast<platform3g::SwitchWindow*>(&input.window);
  if(!window)throw std::invalid_argument("Switch Robot interface requires the real native window");
  caller=std::make_unique<RangersCallerPresentation>(*window,device,RangersCallerPresentation::AcquireWindow{},[window]{
   platform3g::CheckDeviceResult(static_cast<platform3g::DeviceResult>(window->LastVisibilityRestoreResult()),"Rangers caller window visibility restore");
  });
  canvas=std::make_unique<NativeRangersCanvas>(*deko);
#endif
  run=std::make_unique<NativeRangersRunBinding>(input);BuildTable();
 }
 explicit State(const OriginalGameOptions& input):device(RequireDevice(input)){
  run=std::make_unique<NativeRangersRunBinding>(input,RangersLegacyReferenceTag{});BuildTable();
 }
 static platform3g::Device& RequireDevice(const OriginalGameOptions& input){if(!input.device_provider)throw std::invalid_argument("native Robot reference interface has no real device");return *input.device_provider;}
 void BuildTable(){table.Initialize=Robot::TRobotInitialize(&Initialize);table.Finalize=Robot::TRobotAction(&Finalize);table.Support=Robot::TRobotSupportQuery(&Support);table.Run=Robot::TRobotRun(&Run);}
};
thread_local NativeRangersInterface::State* NativeRangersInterface::active_{};
NativeRangersInterface::State& NativeRangersInterface::Current(){if(!active_)throw std::logic_error("native Robot interface is not bound on this thread");return *active_;}
NativeRangersInterface::NativeRangersInterface(const NativeGameOptions& options){
 if(active_ || Robot::RobotInterface || Robot::RobotModule || g_MatrixMap)throw std::logic_error("native Robot interface requires an unbound stopped caller");
 state_=std::make_unique<State>(options);active_=state_.get();
}
NativeRangersInterface::NativeRangersInterface(const OriginalGameOptions& options,RangersLegacyReferenceTag){
 if(active_ || Robot::RobotInterface || Robot::RobotModule || g_MatrixMap)throw std::logic_error("native Robot interface requires an unbound stopped caller");
 state_=std::make_unique<State>(options);active_=state_.get();
}
NativeRangersInterface::~NativeRangersInterface(){
 if(active_!=state_.get())std::terminate();
 try{Finalize();}catch(...){std::terminate();}active_=nullptr;
}
const Robot::TRobotInterfacePrefix& NativeRangersInterface::Table()const noexcept{return state_->table;}
void NativeRangersInterface::Install(const Robot::TRobotCallbacks& input){
 if(active_!=state_.get() || Robot::RobotInterface)throw std::logic_error("native Robot interface already installed or inactive");
 auto copy=input;Initialize(&copy);
 try{const auto result=Support();if(result!=SUPE_OK)throw platform3g::Unsupported("native Robot renderer support code "+std::to_string(result));}
 catch(...){Finalize();throw;}
 Robot::RobotInterface=&state_->table;
}
void PAS_STDCALL NativeRangersInterface::Initialize(Robot::PRobotCallbacks input){
 auto& state=Current();if(!input)throw std::invalid_argument("native Robot Initialize requires callbacks");
 if(state.callbacks || state.services || state.running || g_MatrixMap)throw std::logic_error("native Robot callbacks are already initialized or game is active");
 auto callbacks=std::make_unique<EngineCallbacks>(*input);
 auto services=std::make_unique<NativeRangersServicesScope>(*EngineCallbacks::Table());
 state.callbacks=std::move(callbacks);state.services=std::move(services);
}
void PAS_STDCALL NativeRangersInterface::Finalize(){
 auto& state=Current();if(state.running || g_MatrixMap)throw std::logic_error("native Robot Finalize while game is active");
 if(Robot::RobotInterface && Robot::RobotInterface!=&state.table)throw std::logic_error("native Robot interface replaced by another owner");
 if(Robot::RobotInterface==&state.table)Robot::RobotInterface=nullptr;
 state.services.reset();state.callbacks.reset();
}
std::int32_t PAS_STDCALL NativeRangersInterface::Support(){return ClassifyRangersSupport(QueryRangersSupport(Current().device));}
std::int32_t PAS_STDCALL NativeRangersInterface::Run(std::uint32_t module,std::uint32_t window,char16_t* map,
 Robot::PRobotDisplaySettings settings,char16_t* language,char16_t* start,char16_t* win,char16_t* loss,
 char16_t* planet,aGalaxyStruct::PPlanetBattleStatistics output){
 auto& state=Current();if(!state.callbacks || !state.services)throw std::logic_error("native Robot Run before Initialize");
 if(state.running)throw std::logic_error("recursive native Robot interface Run");
#ifdef __SWITCH__
 if(state.caller && (!state.caller->BattleActive() || state.caller->WindowOwned()))
  throw std::logic_error("native Robot Run requires the original FRun caller window handoff");
#endif
 const auto supported=Support();if(supported!=SUPE_OK)throw platform3g::Unsupported("native Robot Run renderer support code "+std::to_string(supported));
 state.running=true;struct Running{bool& value;~Running(){value=false;}} running{state.running};
 return state.run->Entry()(module,window,map,settings,language,start,win,loss,planet,output);
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_native_input.hpp"
#include "native_input_engine.hpp"
#include "native_ui_target.hpp"
#include "rangers_ui_input.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_host_session.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "ec_file_adapter.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/WindowsSdk.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
namespace m=srhd_awa::matrixgame;
namespace bridge=srhd_awa::platform::rangers_ui_input;
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);
  m::RangersUiContext context;
  const auto was_active=GR_Main::RuntimeActive;
  {
   m::native_input::EngineSink sink;unsigned polls=0,waits=0,warps=0,resets=0;bool drawable=true,attached=true;
   // Scripted HID boundary, actual production owner/GR_WinMessage/GI/imports.
   // This probe does not claim a real SwitchWindow or physical input device.
   m::RangersNativeInputBackend backend;
   backend.poll=[&]{if(polls++==0){sink.Key(true,17);sink.Move(640,360);}};
   backend.reset_input=[&]{assert(attached);++resets;};
   backend.wait_for_drawable=[&]{++waits;drawable=true;};backend.drawable=[&]{return drawable;};backend.extent=[]{return std::pair{1280,720};};
   backend.warp=[&](int x,int y){++warps;sink.Move(x,y);};
   m::RangersNativeInput input(backend);m::RangersScriptSession scripts;m::RangersGlobalUiSession ui;
   auto* loop=reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::screenMainMenu]);
   GI_MessageLoop::PushMessageLoop(loop);
   assert(GR_Main::GR_WinMessage(loop->bind_ProcessWindowMessage())==1 && polls==1 && resets==1);
   assert(GI_MessageLoop::LastMousePosition.X==512 && GI_MessageLoop::LastMousePosition.Y==384);
   assert(WindowsSdk::GetAsyncKeyState(17)&0x8000);
   WindowsSdk::TPoint point{};assert(WindowsSdk::GetCursorPos(point) && point.X==512 && point.Y==384);
   assert(WindowsSdk::ClientToScreen(GR_Main::MainWindowHandle,point) && point.X==512 && point.Y==384);
   assert(WindowsSdk::ScreenToClient(GR_Main::MainWindowHandle,point) && point.X==512 && point.Y==384);
   assert(WindowsSdk::SetCursorPos(800,600) && warps==1);assert(WindowsSdk::GetCursorPos(point));assert(std::abs(point.X-800)<=1 && std::abs(point.Y-600)<=1);
   const auto before_suspend=resets;bridge::Suspend();assert(resets==before_suspend+1 && !bridge::Current() && !m::native_input::CurrentUiTarget());
   bridge::Resume();assert(bridge::Current() && !(WindowsSdk::GetAsyncKeyState(17)&0x8000));
   drawable=false;assert(GR_Main::GR_WinMessage(loop->bind_ProcessWindowMessage())==1 && waits==1);
   sink.Key(true,17);
   auto* child=reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::screenInfo]);
   GI_MessageLoop::PushMessageLoop(child);assert(GR_Main::GR_WinMessage(child->bind_ProcessWindowMessage())==1 && !(WindowsSdk::GetAsyncKeyState(17)&0x8000));
   GI_MessageLoop::PopMessageLoop(child);assert(GR_Main::GR_WinMessage(loop->bind_ProcessWindowMessage())==1);
   const auto before=polls;GR_Main::ExitScreenLoop=true;assert(GR_Main::GR_WinMessage(loop->bind_ProcessWindowMessage())==0 && polls==before);GR_Main::ExitScreenLoop=false;
   GI_MessageLoop::PopMessageLoop(loop);assert(pas::list_count(GI_MessageLoop::MessageLoopStack)==0);pas::free(GI_MessageLoop::MessageLoopStack);GI_MessageLoop::MessageLoopStack=nullptr;
   bridge::Suspend();attached=false; // Simulate an unreacquired borrowed window.
   std::puts("NATIVE_UI_PUMP actual_original_GR_WinMessage_GI=PASS shared_key_cursor_imports=PASS warp_quantization=PASS suspend_resume=PASS nested_owner_queue_cancel=PASS owner_physical_reset=PASS focus_wait=PASS exit_gate=PASS scripted_backend_only=YES");
  }
  assert(GR_Main::RuntimeActive==was_active && !bridge::installed && !m::native_input::CurrentUiTarget());
 }
 assert(!GR_Main::MainDataConfig && !GR_Main::GlobalCache && !Globals::TurnCalculationThread && !Globals::GovernmentScreen);
 std::puts("NATIVE_UI_PUMP original_host_UI_script_cleanup=PASS");
}

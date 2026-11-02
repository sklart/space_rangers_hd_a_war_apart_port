// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_input_queue.hpp"
#include "native_input_engine.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_host_session.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "ec_file_adapter.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/GI_MessageLoop.hpp"
#include "input.hpp"
#include <cassert>
#include <cstdio>
namespace m=srhd_awa::matrixgame;
namespace n=m::native_input;
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);
  m::RangersUiContext context;m::RangersScriptSession scripts;m::RangersGlobalUiSession ui;
  auto* loop=reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::screenMainMenu]);
  assert(loop && loop->RootUiObject && !GR_Main::AlternateViewportEnabled);
  const auto prior=n::ReadEngineKeys();assert(!prior['A'] && !prior[0x7f]);
  m::RangersInputQueue queue(1024,768,1280,720);n::UiTargetScope binding(queue);n::EngineSink sink;
  sink.Key(true,'A');assert(queue.KeyHeld('A') && !Input::isVKeyPressed('A'));assert(queue.Take()->message==0x100);
  sink.Key(false,'A');assert(!queue.KeyHeld('A'));queue.Take();
  sink.Move(640,360);auto event=queue.Take();assert(event && event->message==0x200);
  loop->ProcessWindowMessage(event->message,event->wparam,event->lparam);
  assert(GI_MessageLoop::LastMousePosition.X==512 && GI_MessageLoop::LastMousePosition.Y==384);
  sink.Move(159,0);event=queue.Take();loop->ProcessWindowMessage(event->message,event->wparam,event->lparam);
  assert(GI_MessageLoop::LastMousePosition.X==-1 && GI_MessageLoop::LastMousePosition.Y==-1);
  sink.Mouse(n::Button::Down,1,159,0);assert(!queue.Take() && !queue.KeyHeld(1));
  queue.Enable(false);sink.Key(true,0x7f);assert(Input::isVKeyPressed(0x7f));sink.Key(false,0x7f);assert(!Input::isVKeyPressed(0x7f));
  queue.Enable(true);assert(n::ReadEngineKeys()==prior);
  std::puts("RANGERS_INPUT original_UI_ProcessWindowMessage=PASS aspect_coordinates=512,384 outside=-1,-1 bars_no_click=PASS engine_sink_route=PASS disabled_engine_fallback=PASS engine_keys_preserved=PASS");
 }
 assert(!GR_Main::MainDataConfig && !GR_Main::GlobalCache && !GR_Main::CurrentPixelFormat && !Globals::TurnCalculationThread && !Globals::GovernmentScreen);
 std::puts("RANGERS_INPUT original_host_UI_script_cleanup=PASS");
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_native_input.hpp"
#include "rangers_input_queue.hpp"
#include "rangers_ui_input.hpp"
#include "ui_control_cancel.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Sound.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/GlobalsV.hpp"
#include "units/MMSystem.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace bridge=srhd_awa::platform::rangers_ui_input;
namespace {
GI_MessageLoop::TMessageLoopGI* CurrentLoop(){
 auto* stack=GI_MessageLoop::MessageLoopStack;
 return stack && pas::list_count(stack)?static_cast<GI_MessageLoop::TMessageLoopGI*>(pas::list_get(stack,pas::list_count(stack)-1)):nullptr;
}
bool OnStack(const void* loop){
 auto* stack=GI_MessageLoop::MessageLoopStack;
 for(int i=0;stack && i<pas::list_count(stack);++i)if(pas::list_get(stack,i)==loop)return true;
 return false;
}
RangersNativeInputBackend Validate(RangersNativeInputBackend backend){
 if(!backend.poll || !backend.wait_for_drawable || !backend.reset_input || !backend.drawable || !backend.extent || !backend.warp)
  throw std::invalid_argument("native UI input requires all real window operations");
 if(!GR_Main::CurrentPixelFormat || !GR_Main::ScreenRenderBuffer || GR_Main::AlternateViewportEnabled || !GlobalsV::ScaleViewportToWindow)
  throw std::logic_error("native UI input requires the canonical real software canvas");
 return backend;
}
}
struct RangersNativeInput::Impl final:bridge::Source {
 RangersNativeInputBackend backend;std::pair<int,int> output;
 RangersInputQueue queue;native_input::UiTargetScope target;bridge::Scope registration;
 // Identity only between calls; dereference solely while still on the real stack.
 GI_MessageLoop::TMessageLoopGI* owner{};
 std::uint8_t previous_active=GR_Main::RuntimeActive;
 explicit Impl(RangersNativeInputBackend b):backend(Validate(std::move(b))),output(backend.extent()),
  queue(GR_Main::GameScreenWidth,GR_Main::GameScreenHeight,output.first,output.second),target(queue),registration(*this){}
 bool Enabled()const noexcept override{return queue.Enabled();}
 bool KeyHeld(std::uint8_t key)const noexcept override{return queue.KeyHeld(key);}
 std::pair<int,int> Cursor()const noexcept override{return queue.Position();}
 bool OwnsWindow(std::uint32_t value)const noexcept override{return value==GR_Main::MainWindowHandle;}
 void Cancel(){
  if(owner && OnStack(owner)){
   // Cancel without a mouse-up callback, which could activate an old button.
   if(owner->RootUiObject){
    srhd_awa::platform::controller_screen_context::CancelMomentaryButtons(owner->RootUiObject);
    for(unsigned key=3;key<256;++key)if(queue.KeyHeld(std::uint8_t(key)))owner->RootUiObject->BroadcastKeyUp(key);
   }
  }
  queue.Clear();
  // A suspended source does not own the window; recovery may have failed before
  // reacquisition. Never reset another phase's input during its destructor.
  if(Enabled())backend.reset_input();
 }
 void Suspend()override{if(!Enabled())return;Cancel();queue.Enable(false);GR_Main::RuntimeActive=false;}
 void Resume()override{if(Enabled())return;queue.Enable(true);owner=nullptr;GR_Main::RuntimeActive=backend.drawable();}
 void Warp(int x,int y)override{
  if(!Enabled())throw std::logic_error("native UI warp while battle owns input");
  const auto point=queue.PhysicalPoint(x,y);backend.warp(point.first,point.second);
 }
 bool Pump(const bridge::Dispatch& dispatch)override{
  if(!Enabled())throw std::logic_error("native GI pump while battle owns input");
  if(GR_Main::ExitScreenLoop)return false;
  auto* loop=CurrentLoop();if(!loop)throw std::logic_error("native GI pump requires an original message-loop stack owner");
  if(owner!=loop){Cancel();owner=loop;}
  const auto size=backend.extent();if(size!=output){Cancel();queue.Geometry(GR_Main::GameScreenWidth,GR_Main::GameScreenHeight,size.first,size.second);output=size;}
  backend.poll();
  if(!backend.drawable()){Cancel();backend.wait_for_drawable();}
  if(GR_Main::ExitScreenLoop)return false;
  if(!backend.drawable())throw std::runtime_error("native input wait returned without a drawable window");
  GR_Main::RuntimeActive=true;
  if(GR_Main::SoundManager)GR_Main::SoundManager->UpdateFades();
  // Remove an event before calling original code: dispatch can run nested modals
  // or the whole battle. Owner changes must not replay remaining parent events.
  while(auto event=queue.Take()){
   GR_Main::LastWindowMessageTick=MMSystem::timeGetTime();
   if(GR_Main::MessageIdle && GR_Main::OnMessageResume!=nullptr)GR_Main::OnMessageResume();
   GR_Main::MessageIdle=false;dispatch(event->message,event->wparam,event->lparam);
   if(GR_Main::ExitScreenLoop || loop->ExitCode || CurrentLoop()!=loop || owner!=loop)break;
  }
  const auto elapsed=MMSystem::timeGetTime()-GR_Main::LastWindowMessageTick;
  if(elapsed>5000 && !GR_Main::MessageIdle){GR_Main::MessageIdle=true;if(GR_Main::OnMessageIdle!=nullptr)GR_Main::OnMessageIdle();}
  return !GR_Main::ExitScreenLoop;
 }
 ~Impl(){try{Cancel();GR_Main::RuntimeActive=previous_active;}catch(...){std::terminate();}}
};
RangersNativeInput::RangersNativeInput(RangersNativeInputBackend b):impl_(std::make_unique<Impl>(std::move(b))){}
RangersNativeInput::~RangersNativeInput()=default;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_caller_presentation.hpp"
#include "native_cursor.hpp"
#include "native_diagnostic_journal.hpp"
#include "../../switch/platform/planet_battle_presentation.hpp"
#include "../../switch/platform/rangers_ui_input.hpp"
#include <vector>
namespace srhd_awa::matrixgame {
namespace p=platform3g;
namespace battle=srhd_awa::platform::planet_battle;
struct RangersCallerPresentation::Impl final:battle::Provider {
 p::Window& window;p::Device& device;AcquireWindow acquire;std::function<void()> validate;
 std::unique_ptr<p::WindowAttachment> attachment;bool active{};
 battle::Scope registration;
 Impl(p::Window& w,p::Device& d,AcquireWindow a,std::function<void()> v):window(w),device(d),acquire(std::move(a)),validate(std::move(v)),registration(*this){Acquire();}
 ~Impl(){if(active)std::terminate();if(attachment){attachment->Restore();attachment.reset();}}
 void Acquire(){if(attachment)throw std::logic_error("caller window already attached");auto next=acquire?acquire():window.AttachNative();if(!next)throw std::logic_error("caller window returned no attachment");attachment=std::move(next);if(validate)validate();}
 struct Snapshot final:battle::Lease {
  Impl& owner;std::unique_ptr<p::DeviceStateLease> state;std::vector<p::GammaRamp> gamma;
  native_cursor::Store* native_cursor{};native_cursor::Store::State cursor{};p::CursorHandle system_cursor{};
  bool entered{},restored{};
  explicit Snapshot(Impl& value):owner(value){
   if(owner.active || !owner.attachment)throw std::logic_error("caller capture requires its attached idle window");
   state=owner.device.CaptureState();if(!state)throw std::runtime_error("caller device returned no state lease");
   const auto count=owner.device.ReadCaps().swapchain_count;if(!count || count>32)throw std::runtime_error("caller gamma swapchain count invalid");
   for(unsigned i=0;i<count;++i)gamma.push_back(owner.device.ReadGamma(i));
   if(auto* provider=dynamic_cast<native_cursor::Provider*>(&owner.window)){native_cursor=&provider->CursorStore();cursor=native_cursor->Read();}
   else system_cursor=owner.window.ReadCursor();
   owner.active=true;
  }
  ~Snapshot(){
   if(!restored)try{Restore();}catch(const std::exception& error){diagnostic::Record("failed","Rangers.caller.unwind_restore",error.what());}catch(...){diagnostic::Record("failed","Rangers.caller.unwind_restore","unknown restore failure");}
   owner.active=false;
  }
  bool UsesNativeWindow()const noexcept override{return native_cursor!=nullptr;}
  void EnterBattle()override{
   if(entered || restored || !owner.attachment)throw std::logic_error("caller battle already entered or restored");
   srhd_awa::platform::rangers_ui_input::Suspend();
   entered=true;owner.attachment->Restore();owner.attachment.reset();if(owner.validate)owner.validate();
  }
  void Restore()override{
   if(restored)return; // Only repeated cleanup; the real restore ran already.
   if(!owner.attachment)owner.Acquire();
   p::CheckDeviceResult(state->Restore(),"Rangers caller device/presentation restore");
   for(unsigned i=0;i<gamma.size();++i){
    owner.device.RestoreGamma(i,gamma[i]);const auto read=owner.device.ReadGamma(i);
    if(read.red!=gamma[i].red || read.green!=gamma[i].green || read.blue!=gamma[i].blue)throw std::runtime_error("Rangers caller gamma restore readback differs");
   }
   if(native_cursor)native_cursor->Restore(cursor);else owner.window.ExchangeCursor(system_cursor);
   if(owner.validate)owner.validate();srhd_awa::platform::rangers_ui_input::Resume();state.reset();restored=true;
  }
 };
 std::unique_ptr<battle::Lease> Capture()override{return std::make_unique<Snapshot>(*this);}
};
RangersCallerPresentation::RangersCallerPresentation(p::Window& w,p::Device& d,AcquireWindow a,std::function<void()> v):impl_(std::make_unique<Impl>(w,d,std::move(a),std::move(v))){}
RangersCallerPresentation::~RangersCallerPresentation()=default;
bool RangersCallerPresentation::WindowOwned()const noexcept{return bool(impl_->attachment);}
bool RangersCallerPresentation::BattleActive()const noexcept{return impl_->active;}
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "planet_battle_presentation.hpp"
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace srhd_awa::platform::planet_battle {
// The E2E516 presenter remains the caller until EnterBattle. Operations must
// either complete or leave their previous owner intact; start_native must
// unwind partial construction before throwing. resume_caller may be retried.
// All operations run on the original game/UI thread.
class E2e516Handoff final : public Provider {
public:
 struct Operations {
  std::function<void()> release_caller;
  std::function<void()> start_native;
  std::function<void()> stop_native;
  std::function<void()> resume_caller;
 };
 explicit E2e516Handoff(Operations operations):ops_(std::move(operations)),scope_(*this){
  if(!ops_.release_caller || !ops_.start_native || !ops_.stop_native || !ops_.resume_caller)
   throw std::invalid_argument("E2E516 handoff requires all ownership operations");
 }
 ~E2e516Handoff(){if(captured_)std::terminate();}
 std::unique_ptr<Lease> Capture()override{
  if(captured_)throw std::logic_error("E2E516 battle handoff already captured");
  auto lease=std::make_unique<Snapshot>(*this);captured_=true;return lease;
 }
private:
 struct Snapshot final:Lease {
  E2e516Handoff& owner;
  bool caller_released{},native_started{},finished{};
  explicit Snapshot(E2e516Handoff& value):owner(value){}
  ~Snapshot()override{
   if(!finished)try{Restore();}catch(...){std::terminate();}
   owner.captured_=false;
  }
  bool UsesNativeWindow()const noexcept override{return true;}
  void EnterBattle()override{
   if(finished || caller_released || native_started)throw std::logic_error("E2E516 battle already entered");
   owner.ops_.release_caller();caller_released=true;
   try{owner.ops_.start_native();native_started=true;}
   catch(...){owner.ops_.resume_caller();caller_released=false;throw;}
  }
  void Restore()override{
   if(finished)return;
   if(native_started){owner.ops_.stop_native();native_started=false;}
   if(caller_released){owner.ops_.resume_caller();caller_released=false;}
   finished=true;
  }
 };
 Operations ops_;
 Scope scope_;
 bool captured_{};
};
}

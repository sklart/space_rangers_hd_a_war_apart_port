// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_input.hpp"
#include <stdexcept>
#include <thread>
namespace srhd_awa::matrixgame::native_input {
// An attached caller may receive HID events while the MatrixGame form is absent.
// The caller must disable its target before handing the attachment to a battle.
class UiTarget:public Sink {
public:virtual bool Enabled()const noexcept=0;
};
inline thread_local UiTarget* ui_target{};
inline UiTarget* CurrentUiTarget()noexcept{return ui_target && ui_target->Enabled()?ui_target:nullptr;}
class UiTargetScope {
 UiTarget* target_;std::thread::id thread_=std::this_thread::get_id();
public:
 explicit UiTargetScope(UiTarget& value):target_(&value){
  if(ui_target)throw std::logic_error("native UI input target already owned");
  ui_target=target_;
 }
 ~UiTargetScope(){if(thread_!=std::this_thread::get_id() || ui_target!=target_)std::terminate();ui_target=nullptr;}
 UiTargetScope(const UiTargetScope&)=delete;UiTargetScope& operator=(const UiTargetScope&)=delete;
};
}

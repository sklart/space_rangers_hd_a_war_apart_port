// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdexcept>
#include <utility>
namespace srhd_awa::platform::renderer_platform {
// The caller supplies its actual SDL destruction operation. A suspended
// presenter cannot submit another frame until the native owner has stopped.
class SdlPresentationHandoff {
public:
 template<class Release>void Suspend(Release&& release){
  if(suspended_)throw std::logic_error("SDL presentation already suspended");
  std::forward<Release>(release)();suspended_=true;
 }
 void Resume(){
  if(!suspended_)throw std::logic_error("SDL presentation is not suspended");
  suspended_=false;
 }
 bool Suspended()const noexcept{return suspended_;}
private:bool suspended_{};
};
}

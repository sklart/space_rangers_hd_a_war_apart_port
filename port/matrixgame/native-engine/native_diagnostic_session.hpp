// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_diagnostic_control.hpp"
namespace srhd_awa::matrixgame::diagnostic {
enum class SessionMode {Quick,Interactive};
struct ExitButtons {bool b{},plus{},minus{};};
// This controls diagnostic lifetime only. It does not synthesize game input.
class SessionControl {
public:
 static constexpr std::uint64_t TimeoutUs=120'000'000,MinimumFrames=4;
 SessionControl(SessionMode mode,std::uint64_t now):mode_(mode),control_(now,TimeoutUs,MinimumFrames){}
 RunControl::PollResult Poll(std::uint64_t now,ExitButtons buttons){
  const bool exit=mode_==SessionMode::Quick?(buttons.b || buttons.plus):(buttons.minus && buttons.plus);
  last_exit_=exit;sampled_=true;
  return control_.Poll(now,exit);
 }
 RunControl::PollResult BeginOriginalLoop(std::uint64_t now){
  if(loop_)throw std::logic_error("diagnostic original loop already started");
  if(!sampled_)throw std::logic_error("diagnostic phase requires an actual exit-button sample");
  // Validate the monotonic clock before changing the phase anchor.
  const auto prior=control_.Poll(now,last_exit_);
  if(prior!=RunControl::PollResult::Continue)return prior;
  if(mode_==SessionMode::Interactive){control_=RunControl(now,TimeoutUs,MinimumFrames);(void)control_.Poll(now,last_exit_);}
  loop_=true;return prior;
 }
 bool Presented(std::uint64_t before,std::uint64_t after,bool map_visible){
  if(!loop_)throw std::logic_error("diagnostic presentation before original loop");
  if(after<before)throw std::logic_error("diagnostic presentation counter moved backwards");
  if(presentation_sampled_ && before<last_present_)throw std::logic_error("diagnostic presentation counter reused an earlier frame");
  if(!map_visible){last_present_=after;presentation_sampled_=true;return false;}
  const auto enough=control_.Presented(before,after);
  last_present_=after;presentation_sampled_=true;
  return mode_==SessionMode::Quick && enough;
 }
 bool HasControlledDeadline()const noexcept{return loop_ && mode_==SessionMode::Interactive;}
 std::uint64_t Frames()const noexcept{return control_.Frames();}
 bool ValidLoopReturn(int state)const noexcept{return Frames()>=MinimumFrames && (mode_==SessionMode::Quick?state==0:state>=0 && state<=4);}
 bool CountersDue()const noexcept{return mode_==SessionMode::Quick || Frames()<=MinimumFrames || Frames()%60==0;}
 SessionMode Mode()const noexcept{return mode_;}
private:
 SessionMode mode_;RunControl control_;std::uint64_t last_present_{};bool loop_{},sampled_{},last_exit_{},presentation_sampled_{};
};
}

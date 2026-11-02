#ifdef NDEBUG
#error Diagnostic session verification requires assertions
#endif
#include "native_diagnostic_session.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
namespace d=srhd_awa::matrixgame::diagnostic;using R=d::RunControl::PollResult;
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(){
 unsigned modes{},button_cases{};
 for(auto mode:{d::SessionMode::Quick,d::SessionMode::Interactive}){
  for(unsigned mask=0;mask<8;++mask){
   d::SessionControl control(mode,10);assert(control.Poll(10,{})==R::Continue);
   const bool b=mask&1,plus=mask&2,minus=mask&4;
   const bool exit=mode==d::SessionMode::Quick?(b||plus):(plus&&minus);
   assert(control.Poll(11,{b,plus,minus})==(exit?R::UserExit:R::Continue));++button_cases;
  }
  d::SessionControl control(mode,100);Reject([&]{control.BeginOriginalLoop(101);});Reject([&]{control.Presented(0,1,true);});
  assert(control.Poll(101,{})==R::Continue);assert(control.BeginOriginalLoop(110)==R::Continue);Reject([&]{control.BeginOriginalLoop(111);});
  assert(!control.Presented(0,12,false) && control.Frames()==0 && !control.ValidLoopReturn(0));
  assert(!control.Presented(12,12,true) && control.Frames()==0);
  for(unsigned frame=1;frame<=120;++frame){
   assert(control.Presented(12+frame-1,12+frame,true)==(mode==d::SessionMode::Quick && frame>=4));
   assert(control.Frames()==frame);
   assert(control.CountersDue()==(mode==d::SessionMode::Quick || frame<=4 || frame%60==0));
   for(int exit=-1;exit<=5;++exit)assert(control.ValidLoopReturn(exit)==(frame>=4 && (mode==d::SessionMode::Quick?exit==0:exit>=0 && exit<=4)));
   for(int exit=100;exit<=104;++exit)assert(!control.ValidLoopReturn(exit));
  }
  Reject([&]{control.Presented(2,1,true);});Reject([&]{control.Presented(2,1,false);});Reject([&]{control.Poll(109,{});});++modes;
  Reject([&]{control.Presented(131,132,true);});assert(control.Frames()==120);
 }
 // A held startup exit is never manufactured into a fresh press by a phase.
 for(auto mode:{d::SessionMode::Quick,d::SessionMode::Interactive}){
  const d::ExitButtons held{true,true,true};d::SessionControl control(mode,0);
  assert(control.Poll(0,held)==R::Continue);assert(control.BeginOriginalLoop(1)==R::Continue);assert(control.Poll(2,held)==R::Continue);
  assert(control.Poll(3,{})==R::Continue);assert(control.Poll(4,held)==R::UserExit);
 }
 d::SessionControl quick(d::SessionMode::Quick,0),manual(d::SessionMode::Interactive,0);
 assert(quick.Poll(1,{})==R::Continue && manual.Poll(1,{})==R::Continue);
 assert(quick.BeginOriginalLoop(119'000'000)==R::Continue && manual.BeginOriginalLoop(119'000'000)==R::Continue);
 assert(quick.Poll(120'000'000,{})==R::Deadline);assert(manual.Poll(120'000'000,{})==R::Continue);
 assert(manual.Poll(238'999'999,{})==R::Continue && manual.Poll(239'000'000,{})==R::Deadline);
 d::SessionControl expired(d::SessionMode::Interactive,0);assert(expired.Poll(1,{})==R::Continue);
 assert(expired.BeginOriginalLoop(120'000'000)==R::Deadline);Reject([&]{expired.Presented(0,1,true);});
 d::SessionControl clock(d::SessionMode::Interactive,100);assert(clock.Poll(101,{})==R::Continue);Reject([&]{clock.BeginOriginalLoop(100);});
 std::printf("DIAGNOSTIC_SESSION modes=%u button_cases=%u quick_four_frames=PASS manual_no_autostop=PASS B_Plus_game_controls=PASS fresh_chord_arming=PASS phase_deadlines=PASS counters_throttled=PASS transition_excluded=PASS exit_states=PASS surrender4=PASS exception100_104_rejected=PASS invalid_sequences=PASS CPU_POLICY_ONLY NO_GPU\n",modes,button_cases);
}

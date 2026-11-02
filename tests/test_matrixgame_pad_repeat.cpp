// SPDX-License-Identifier: GPL-2.0-or-later
// Input dispatch counters only; actual robot behavior has a separate HAL test.
#include "native_input.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
namespace n=srhd_awa::matrixgame::native_input;
struct Sink final:n::Sink {
 std::vector<std::pair<bool,unsigned>> events;std::bitset<256> held;
 void Key(bool down,std::uint8_t key)override{events.emplace_back(down,key);held[key]=down;}
 void Move(int,int)override{}
 void Mouse(n::Button,int,int,int)override{}
 void ReconcileMouseKeys(const std::bitset<256>&,unsigned)noexcept override{}
 unsigned Downs()const{unsigned count=0;for(auto [down,key]:events)count+=down;return count;}
 unsigned Ups()const{unsigned count=0;for(auto [down,key]:events)count+=!down;return count;}
};
int main(int argc,char** argv){
 {Sink sink;n::Router router(sink);router.Focus(true);n::Sample held;held.keys.set(13);router.Feed(n::Source::Pad,held,100);
  router.Repeat(600);router.Repeat(633);router.Repeat(10000);
  if(argc==2 && !std::strcmp(argv[1],"--baseline")){assert(sink.Downs()>1);std::printf("BASELINE pad held produced repeated KeyDown events=%u: REPRODUCED\n",sink.Downs());return 0;}
  assert(sink.Downs()==1 && sink.held[13]);router.Feed(n::Source::Pad,{},10001);assert(sink.Ups()==1 && sink.held.none());}
 // Physical keyboard typematic is retained, including bounded catch-up.
 {Sink sink;n::Router router(sink);router.Focus(true);n::Sample held;held.keys.set('A');router.Feed(n::Source::Keyboard,held,0);
  router.Feed(n::Source::Keyboard,held,400);router.Repeat(499);assert(sink.Downs()==1);router.Repeat(500);assert(sink.Downs()==2);
  router.Repeat(533);assert(sink.Downs()==3);router.Repeat(550);assert(sink.Downs()==3);router.Repeat(10000);assert(sink.Downs()==7);}
 // Joining an already-held pad key starts the keyboard's own 500ms delay.
 {Sink sink;n::Router router(sink);router.Focus(true);n::Sample held;held.keys.set('A');router.Feed(n::Source::Pad,held,0);
  router.Feed(n::Source::Keyboard,held,400);router.Repeat(600);router.Repeat(899);assert(sink.Downs()==1);
  router.Repeat(900);assert(sink.Downs()==2);router.Feed(n::Source::Keyboard,{},950);router.Repeat(1200);assert(sink.Downs()==2 && sink.Ups()==0 && sink.held['A']);
  router.Feed(n::Source::Keyboard,held,1300);router.Repeat(1799);assert(sink.Downs()==2);router.Repeat(1800);assert(sink.Downs()==3);
  router.Feed(n::Source::Pad,{},1810);assert(sink.Ups()==0);router.Feed(n::Source::Keyboard,{},1820);assert(sink.Ups()==1);}
 // A pad joining/releasing must neither reset nor steal keyboard typematic.
 {Sink sink;n::Router router(sink);router.Focus(true);n::Sample held;held.keys.set('W');router.Feed(n::Source::Keyboard,held,100);
  router.Feed(n::Source::Pad,held,450);router.Repeat(600);assert(sink.Downs()==2);
  router.Feed(n::Source::Pad,{},610);router.Repeat(633);assert(sink.Downs()==3 && sink.Ups()==0);router.Focus(false);assert(sink.Ups()==1 && sink.held.none());
  router.Repeat(10000);router.Focus(true);router.Repeat(20000);assert(sink.Downs()==3);}
 for(unsigned modifier:{16u,17u,18u,91u,92u}){Sink sink;n::Router router(sink);router.Focus(true);n::Sample held;held.keys.set(modifier);
  router.Feed(n::Source::Keyboard,held,0);router.Repeat(10000);assert(sink.Downs()==1);}
 std::puts("pad edge-only held state; keyboard delay/rate/catch-up; merged-source ownership and independent repeat timer; modifiers/focus PASS CPU_INPUT_DISPATCH_ONLY NO_HID NO_SWITCH");
}

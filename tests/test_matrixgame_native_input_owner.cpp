// SPDX-License-Identifier: GPL-2.0-or-later
#include "../port/matrixgame/native-engine/native_input_gate.hpp"
#include "../port/matrixgame/native-engine/rangers_input_queue.hpp"
#include <cassert>
#include <cstdio>
namespace m=srhd_awa::matrixgame;
int main(){
 for(unsigned initial=0;initial<256;++initial)for(unsigned next=0;next<256;++next){
  m::native_input::HeldUntilRelease<unsigned> gate;gate.Prime(initial);
  assert(gate.Filter(next)==(next&~initial));
  assert(gate.Filter(next)==(next&~initial));
  assert(gate.Filter(0)==0 && gate.Filter(next)==next);
 }
 m::native_input::HeldUntilRelease<std::bitset<256>> keys;std::bitset<256> held;held.set(255);held.set(17);keys.Prime(held);
 auto next=held;next.set('A');assert(keys.Filter(next).count()==1 && keys.Filter(next)['A']);
 keys.Filter({});assert(keys.Filter(next)==next);
 m::RangersInputQueue q(1024,768,1280,720);m::native_input::Router router(q);router.Extent(1280,720);router.Warp(640,360);
 assert(router.X()==640 && router.Y()==360 && q.Position()==std::pair(512,384));
 for(int y=0;y<768;++y)for(int x=0;x<1024;++x){auto physical=q.PhysicalPoint(x,y);q.Move(physical.first,physical.second);auto logical=q.Position();assert(std::abs(logical.first-x)<=1 && std::abs(logical.second-y)<=1);}
 q.Geometry(1024,768,1920,1080);assert(!q.Take());auto p=q.PhysicalPoint(512,384);q.Move(p.first,p.second);assert(q.Position()==std::pair(512,384));
 std::puts("NATIVE_INPUT_OWNER_PRIMITIVES held_release_65536_cases=PASS keyboard_high_bits=PASS router_warp=PASS all_canvas_warp_points=PASS geometry_change=PASS");
}

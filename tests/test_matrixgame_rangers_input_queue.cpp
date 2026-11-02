// SPDX-License-Identifier: GPL-2.0-or-later
#include "../port/matrixgame/native-engine/rangers_input_queue.hpp"
#include <cassert>
#include <climits>
#include <thread>
#include <cstdio>
namespace m=srhd_awa::matrixgame;
namespace n=m::native_input;
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(...){rejected=true;}assert(rejected);}
int main(){
 Reject([]{m::RangersInputQueue q(INT_MAX,768,1280,720);});
 Reject([]{m::RangersInputQueue q(1024,0,1280,720);});
 m::RangersInputQueue q(1024,768,1280,720);
 // Exact same 960x720 image at x=160 as the real RGB565 presenter.
 for(int y=0;y<720;++y)for(int x=160;x<1120;++x){
  q.Move(x,y);auto p=q.Position();assert(p.first==(x-160)*1024/960 && p.second==y*768/720);
 }
 assert(q.Pending()==1);auto move=q.Take();assert(move && move->message==0x200 && !q.Take());
 q.Mouse(n::Button::Down,1,159,50);assert(!q.Take() && !q.KeyHeld(1));
 q.Move(160,0);q.Take();q.Mouse(n::Button::Down,1,160,0);
 auto down=q.Take();assert(down && down->message==0x201 && down->lparam==0 && (down->wparam&1));
 q.Move(1120,50);q.Take();q.Mouse(n::Button::Up,1,1120,50);
 auto up=q.Take();assert(up && up->message==0x202 && up->lparam==-1 && !q.KeyHeld(1));
 q.Mouse(n::Button::Double,1,640,360);q.ReconcileMouseKeys({},0);
 assert(q.KeyHeld(1) && q.Take()->message==0x203);q.Mouse(n::Button::Up,1,640,360);q.Take();
 q.Key(true,16);q.Take();q.Key(true,17);q.Take();q.Mouse(n::Button::Wheel,-120,640,360);
 auto wheel=q.Take();assert(wheel && wheel->message==0x20a && std::int16_t(wheel->wparam>>16)==-120 && (wheel->wparam&12)==12);
 Reject([&]{q.Mouse(n::Button::Wheel,32768,640,360);});
 Reject([&]{q.Mouse(n::Button::Down,3,640,360);});
 q.Clear();assert(!q.KeyHeld(16) && !q.KeyHeld(17) && !q.Take());
 {
  n::UiTargetScope scope(q);assert(n::CurrentUiTarget()==&q);
  Reject([&]{n::UiTargetScope duplicate(q);});
  std::thread foreign([]{assert(!n::CurrentUiTarget());});foreign.join();
  n::Router router(q);router.Extent(1280,720);router.Focus(true);
  n::Sample keyboard;keyboard.keys.set('A');router.Feed(n::Source::Keyboard,keyboard,0);
  n::Sample pad=keyboard;router.Feed(n::Source::Pad,pad,1);router.Feed(n::Source::Keyboard,{},2);
  assert(q.KeyHeld('A'));router.Feed(n::Source::Pad,{},3);assert(!q.KeyHeld('A'));
  router.Focus(false);q.Enable(false);assert(!n::CurrentUiTarget() && !q.Take());
  q.Enable(true);assert(n::CurrentUiTarget()==&q);
 }
 assert(!n::CurrentUiTarget());
 for(int i=0;i<2048;++i)q.Key(true,'A');Reject([&]{q.Key(true,'A');});q.Clear();
 std::puts("RANGERS_INPUT exhaustive_letterbox_points=PASS bars_no_click=PASS drag_release=PASS double_modifiers_wheel=PASS source_merge=PASS scoped_route=PASS bounded_queue=PASS");
}

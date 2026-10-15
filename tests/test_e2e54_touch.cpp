#include "input_platform.hpp"
#include <cassert>
using namespace srhd_awa::platform::input_platform;
int main() {
  State s; RawInput input{};
  auto idle=NormalizeAt(&s,input,1280,720,0);
  input.touch=true; input.touch_x=100; input.touch_y=200;
  auto down=NormalizeAt(&s,input,1280,720,10000);
  assert(down.x==100 && down.y==200 && down.left_down && down.left_held);
  input.touch_x=1279; input.touch_y=719;
  auto drag=NormalizeAt(&s,input,1280,720,20000);
  assert(drag.x==1279 && drag.y==719 && !drag.left_down && drag.left_held);
  input.a=true; input.touch=false;
  auto handoff=NormalizeAt(&s,input,1280,720,30000);
  assert(handoff.left_held && !handoff.left_up && !handoff.left_down);
  input.a=false;
  auto up=NormalizeAt(&s,input,1280,720,40000);
  assert(up.left_up && !up.left_held);
  input.touch=true; input.touch_x=2000; input.touch_y=-50;
  auto bounds=NormalizeAt(&s,input,640,360,50000);
  assert(bounds.x==639 && bounds.y==0 && bounds.left_down);
  State applet;
  SuppressUntilReleased();
  RawInput confirm{}; confirm.a=true;
  InjectHostRaw(&applet,confirm);
  assert(!PollHistory(&applet,1280,720).snapshots[0].left_down);
  assert(!PollHistory(&applet,1280,720).snapshots[0].left_down);
  InjectHostRaw(&applet,{});
  assert(!PollHistory(&applet,1280,720).snapshots[0].left_down);
  InjectHostRaw(&applet,confirm);
  assert(PollHistory(&applet,1280,720).snapshots[0].left_down);
}

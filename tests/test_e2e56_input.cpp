#include "input_platform.hpp"
#include "e2e56_arcade_steering.hpp"
#include <cassert>
#include <cmath>
using namespace srhd_awa::platform;
int main() {
  using namespace input_platform;
  State s; RawInput raw{};
  NormalizeAt(&s, raw, 1280, 720, 0);
  SetArcadeControls(true);
  NormalizeAt(&s,raw,1280,720,0);
  raw.stick_x=32767;
  auto sample=NormalizeAt(&s,raw,1280,720,50000);
  assert(sample.x==640 && GetFlightStick().x==32767);
  raw.right_stick_y=32767;
  sample=NormalizeAt(&s,raw,1280,720,100000);
  assert(sample.x==640 && sample.y<360 && GetFlightStick().x==32767);
  raw.touch=true; raw.touch_x=300;raw.touch_y=200;
  sample=NormalizeAt(&s,raw,1280,720,150000);
  assert(sample.x==300 && sample.y==200 && sample.left_down && GetFlightStick().x==32767);
  raw={};raw.stick_x=3000;raw.stick_y=-3000;
  NormalizeAt(&s,raw,1280,720,200000);
  assert(!GetFlightStick().x && !GetFlightStick().y);
  SetArcadeControls(false);
  raw = {};
  NormalizeAt(&s,raw,1280,720,200000);
  raw.stick_x=32767;raw.stick_y=0;
  sample=NormalizeAt(&s,raw,1280,720,250000);
  assert(sample.x>300 && !GetFlightStick().x);
  using e2e56_arcade_steering::Steer;
  assert(Steer(0,0,32767).thrust && Steer(0,0,32767).turn==0);
  assert(!Steer(0,32767,0).thrust && Steer(0,32767,0).turn==90);
  assert(Steer(90,32767,0).thrust && Steer(90,32767,0).turn==0);
  assert(Steer(180,0,-32767).thrust && Steer(180,0,-32767).turn==0);
  assert(Steer(270,-32767,0).thrust && Steer(270,-32767,0).turn==0);
  assert(std::abs(Steer(359,0,32767).turn-1)<1e-12);
  assert(std::abs(Steer(1,0,32767).turn+1)<1e-12);
  assert(!Steer(0,0,0).thrust);
  // Exercise the actual turn/thrust command through 180 degrees, using the
  // original ship's 3.3 degree/tick bound: no overshoot at the requested heading.
  double heading=0; int thrust_ticks=0;
  for(int tick=0;tick<80;++tick) {
    const auto command=Steer(heading,0,-32767);
    heading+=std::clamp(command.turn,-3.3,3.3);
    if(command.thrust)++thrust_ticks;
  }
  assert(std::abs(std::abs(heading)-180)<1e-10 && thrust_ticks>20);
}

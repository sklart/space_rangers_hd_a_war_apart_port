#include "input_platform.hpp"
#include <cassert>
using namespace srhd_awa::platform::input_platform;
int main() {
  State s; RawInput raw{};NormalizeAt(&s,raw,1280,720,0);
  SetArcadeControls(true);
  NormalizeAt(&s,raw,1280,720,0); // Establish neutral state in the new owner.
  raw.dpad_up=raw.dpad_left=raw.zr=raw.zl=true;
  auto flight=NormalizeAt(&s,raw,1280,720,10000);
  assert(flight.flight_keys==53 && flight.x==640 && flight.y==360);
  raw.minus=true;auto chord=NormalizeAt(&s,raw,1280,720,20000);
  assert(chord.quicksave_down && chord.flight_keys==5);
  SetArcadeControls(false);
  raw={};auto released=NormalizeAt(&s,raw,1280,720,30000);assert(!released.flight_keys);
  raw.dpad_right=true;auto cursor=NormalizeAt(&s,raw,1280,720,40000);
  assert(cursor.x>released.x && !cursor.flight_keys);
}

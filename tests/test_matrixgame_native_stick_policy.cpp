#include "../port/matrixgame/native-engine/native_stick_policy.hpp"
#include <cassert>
using srhd_awa::matrixgame::native_input::CameraScroll;
using srhd_awa::matrixgame::native_input::MapBattleSticks;
int main(){
 auto d=CameraScroll({12000,-12000});assert(!d.left&&!d.right&&!d.up&&!d.down);
 d=CameraScroll({-12001,12001});assert(d.left&&d.up&&!d.right&&!d.down);
 d=CameraScroll({32767,-32767});assert(d.right&&d.down&&!d.left&&!d.up);
 d=CameraScroll({0,0});assert(!d.left&&!d.right&&!d.up&&!d.down);
 const auto split=MapBattleSticks({1234,-4321},{-20000,20000});
 assert(split.pointer.x==1234 && split.pointer.y==-4321 && split.camera.left && split.camera.up);
 const auto independent=MapBattleSticks({-30000,30000},{0,0});
 assert(independent.pointer.x==-30000 && !independent.camera.left && !independent.camera.up);
}

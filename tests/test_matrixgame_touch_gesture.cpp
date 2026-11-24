#include "native_touch_gesture.hpp"
#include "native_input.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame::native_input;
struct TestSink:Sink{
 unsigned downs{},ups{},wheel{};std::bitset<256> held;
 void Key(bool down,std::uint8_t k)override{held[k]=down;}
 void Move(int,int)override{}
 void Mouse(Button b,int,int,int)override{if(b==Button::Down)++downs;if(b==Button::Up)++ups;if(b==Button::Wheel)++wheel;}
 void ReconcileMouseKeys(const std::bitset<256>&,unsigned)noexcept override{}
};
int main(){
 TouchGesture g;g.Begin(100,100);auto m=g.Move(105,107);assert(!m.dx && !m.dy && g.End());
 g.Begin(100,100);m=g.Move(120,94);assert(m.dx==20 && m.dy==-6);
 auto dirs=PanToCamera(m.dx,m.dy);assert(dirs.left && dirs.down && !dirs.right && !dirs.up);
 m=g.Move(115,110);assert(m.dx==-5 && m.dy==16);dirs=PanToCamera(m.dx,m.dy);assert(dirs.right && dirs.up);
 assert(!g.End());g.Begin(2,3);g.Cancel();assert(!g.End());
 ZoomRepeat zoom;assert(zoom.Update(true,false,1000)==1);assert(!zoom.Update(true,false,1219));
 assert(zoom.Update(true,false,1220)==1);assert(zoom.Update(false,true,1221)==-1);
 assert(!zoom.Update(true,true,1250));assert(zoom.Update(false,true,1260)==-1);
 zoom.Reset();assert(zoom.Update(true,false,5)==1);assert(zoom.Update(true,false,2)==1);
 TestSink sink;Router router(sink);router.Extent(1280,720);router.Focus(true);
 Sample tap;tap.point=true;tap.x=400;tap.y=300;tap.buttons=1;router.Feed(Source::Touch,tap,100);router.Feed(Source::Touch,{},101);
 assert(sink.downs==1 && sink.ups==1);
 Sample pad;pad.keys.set(37);router.Feed(Source::Pad,pad,200);Sample swipe;swipe.keys.set(37);router.Feed(Source::Touch,swipe,200);
 router.Feed(Source::Touch,{},201);assert(sink.held[37]);router.Feed(Source::Pad,{},202);assert(!sink.held[37]);
 Sample z;z.wheel=1;router.Feed(Source::Pad,z,300);assert(sink.wheel==1);
 router.Focus(false);assert(!sink.held.any());
 std::puts("tap, swipe threshold/direction, zoom repeat and source release PASS; CPU ONLY");
}

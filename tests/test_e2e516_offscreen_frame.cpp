#include "../port/matrixgame/native-engine/native_frame_scope.hpp"
#include <cassert>
#include <stdexcept>
#include <vector>
using srhd_awa::matrixgame::platform3g::NativeFrameScope;
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::logic_error&){rejected=true;}assert(rejected);}
int main(){
 NativeFrameScope frame;
 assert(!frame.CanAdoptOffscreen(false,false));
 frame.Acquire();
 assert(frame.CanAdoptOffscreen(true,false));
 assert(!frame.CanAdoptOffscreen(false,false));
 assert(!frame.CanAdoptOffscreen(true,true));
 std::vector<int> pending{1,2}; // Pre-frame offscreen work must remain ordered.
 if(frame.CanAdoptOffscreen(true,false))pending.push_back(3);
 assert((pending==std::vector<int>{1,2,3}));
 frame.BeginScene();assert(!frame.CanAdoptOffscreen(true,false));
 Reject([&]{frame.RequirePresent();});
 frame.EndScene();assert(frame.CanAdoptOffscreen(true,false));
 frame.Submitted();assert(!frame.CanAdoptOffscreen(true,false));
 frame.Presented();assert(!frame.CanAdoptOffscreen(true,false));
}

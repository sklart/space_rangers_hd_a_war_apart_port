#ifdef NDEBUG
#error Keep benchmark assertions
#endif
#include "native_device_state.hpp"
#include <chrono>
#include <cstdio>
#include <cassert>
namespace p=srhd_awa::matrixgame::platform3g;
int main(){p::NativeRenderState state,control;control.vertex.Layout(0x112);std::uint64_t hash=1469598103934665603ull;
 constexpr unsigned values[]{0,1,3,1,0,2,3,2,0x3e800000,0x12345678,8,1,0,0};
 auto setters=[&](unsigned n){for(unsigned k=0;k<n;++k)for(unsigned slot=0;slot<8;++slot)for(unsigned id=1;id<=13;++id){state.Sampler(slot,id,values[id]);hash=(hash^state.sampler_extra[slot][0])*1099511628211ull;}};
 auto getters=[&](unsigned n){for(unsigned k=0;k<n;++k)for(unsigned slot=0;slot<8;++slot)for(unsigned id=1;id<=13;++id)hash=(hash^state.Sampler(slot,id))*1099511628211ull;};
 setters(1000);getters(1000);for(unsigned i=0;i<10000;++i)(void)control.PrepareDraw();
 auto start=std::chrono::steady_clock::now();setters(32768);auto set=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();getters(32768);auto get=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();for(unsigned i=0;i<100000;++i){auto draw=control.PrepareDraw();hash=(hash^draw.fog.controls[0])*1099511628211ull;}auto prepare=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();assert(hash);
 std::printf("SAMPLER_PROFILE setters=3407872 getters=3407872 prepares=100000 set_us=%lld get_us=%lld prepare_us=%lld checksum=%llu CPU_SAMPLER_ONLY_NOT_GPU\n",(long long)set,(long long)get,(long long)prepare,(unsigned long long)hash);
}


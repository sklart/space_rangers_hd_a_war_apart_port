// Actual production vertex decoder/state validation. CPU-only workload;
// compilation comparison, not a battle or Switch GPU benchmark.
#ifdef NDEBUG
#error This diagnostic benchmark must retain assertions
#endif
#include "native_device_state.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
namespace p=srhd_awa::matrixgame::platform3g;
using Clock=std::chrono::steady_clock;
template<class F>void Reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
int main(){
 p::NativeStateStore store;auto& state=store.Edit();state.vertex.Layout(0x112);state.vertex.Viewport({0,0,1280,720,0,1});
 for(unsigned reg:{1u,4u})state.combiner.Set(0,reg,2);
 for(unsigned reg:{2u,5u})state.combiner.Set(0,reg,0);
 std::uint64_t hash=1469598103934665603ull;auto word=[&](float f){std::uint32_t w;std::memcpy(&w,&f,4);hash=(hash^w)*1099511628211ull;};
 auto start=Clock::now();
 for(unsigned i=0;i<20000;++i){state.ValidateDraw();auto v=state.vertex.Prepare();word(v.clip[i%16]);}
 const auto validate=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
 std::array<float,8> input{.125f,.25f,.5f,0,0,1,.2f,.3f};start=Clock::now();
 for(unsigned i=0;i<200000;++i){input[0]=float(i%1024)/1024;const auto v=state.vertex.DecodeOne(std::as_bytes(std::span{input}),sizeof(input));word(v.position[0]);word(v.uv[0][1]);}
 const auto decode=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
 Reject([&]{state.ValidateDraw(true);});Reject([&]{state.Render(29,1);});
 input[0]=std::numeric_limits<float>::quiet_NaN();Reject([&]{state.vertex.DecodeOne(std::as_bytes(std::span{input}),sizeof(input));});
 store.Frame(true);Reject([&]{store.Capture();});store.Frame(false);
 std::printf("NATIVE_CPU_PROFILE validate_calls=20000 decode_vertices=200000 validate_us=%lld decode_us=%lld checksum=%llu guards=4 ASSERTIONS_ON CPU_ONLY_NOT_SWITCH\n",(long long)validate,(long long)decode,(unsigned long long)hash);
}

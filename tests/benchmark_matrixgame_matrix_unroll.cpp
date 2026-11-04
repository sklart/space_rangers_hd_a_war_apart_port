#ifdef NDEBUG
#error Keep benchmark assertions
#endif
#include "fixed_vertex.hpp"
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdio>
namespace mat=srhd_awa::matrixgame::materials;
int main(){
 using Clock=std::chrono::steady_clock;
 mat::Matrix a=mat::Identity(),b=mat::Identity();b[1]=.125f;b[6]=.25f;b[11]=-.125f;
 std::uint64_t hash=1469598103934665603ull;auto mix=[&](float v){hash=(hash^std::bit_cast<unsigned>(v))*1099511628211ull;};
 auto start=Clock::now();
 for(unsigned i=0;i<10000000;++i){a[12]=float(i%1024)/1024;const auto out=mat::Multiply(a,b);mix(out[12]);mix(out[13]);}
 auto multiply=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
 mat::FixedVertexState state;state.Layout(0x112);state.Transform(2,b);state.Transform(3,b);start=Clock::now();
 for(unsigned i=0;i<3000000;++i){if(i%1024==0){a[12]=float(i%8192)/8192;state.Transform(256,a);}const auto u=state.Prepare();mix(u.camera[12]);mix(u.clip[13]);mix(u.normal[0]);}
 auto prepare=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
 assert(hash);std::printf("MATRIX_PROFILE multiplies=10000000 prepares=3000000 multiply_us=%lld prepare_us=%lld checksum=%llu CPU_UNIFORM_PREPARATION_ONLY_NOT_GPU\n",(long long)multiply,(long long)prepare,(unsigned long long)hash);
}

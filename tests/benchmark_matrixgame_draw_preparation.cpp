#ifdef NDEBUG
#error Benchmark requires assertions
#endif
#include "matrixgame_draw_preparation_fixture.hpp"
#include <chrono>
#include <cstdio>
int main(){p::NativeRenderState s;s.vertex.Layout(0x112);s.vertex.Viewport({0,0,1280,720,0,1});for(unsigned r:{1u,4u})s.combiner.Set(0,r,2);for(unsigned r:{2u,5u})s.combiner.Set(0,r,0);
 std::uint64_t checksum=1469598103934665603ull;auto start=std::chrono::steady_clock::now();
 for(unsigned i=0;i<100000;++i){auto d=Prepare(s);checksum=(checksum^std::to_integer<unsigned>(d.packet[i%d.packet.size()]))*1099511628211ull;}
 auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 std::printf("DRAW_PREPARATION calls=100000 prepare_us=%lld checksum=%llu CPU_ONLY_NOT_SWITCH\n",(long long)us,(unsigned long long)checksum);
}

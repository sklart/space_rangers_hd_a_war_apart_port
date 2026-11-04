#ifdef NDEBUG
#error Keep benchmark assertions
#endif
#include "native_device_state.hpp"
#include "native_state_layout.hpp"
#include <chrono>
#include <cstdio>
#include <cassert>
namespace p=srhd_awa::matrixgame::platform3g;
int main(){p::NativeRenderState state;state.vertex.Layout(0x112);std::uint64_t hash=1469598103934665603ull;
 // Warm the same state/draw paths in both variants before measuring.
 for(const auto& w:p::NativeRenderWords)state.Render(w.id,w.initial);
 for(unsigned n=0;n<10000;++n)(void)state.PrepareDraw();
 auto start=std::chrono::steady_clock::now();
 for(unsigned n=0;n<16384;++n)for(const auto& w:p::NativeRenderWords){state.Render(w.id,w.initial);hash=(hash^state.registers.at(w.id))*1099511628211ull;}
 auto setter=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();for(unsigned n=0;n<100000;++n){const auto draw=state.PrepareDraw();hash=(hash^draw.fog.controls[0])*1099511628211ull;}
 auto prepare=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();assert(hash);
 std::printf("SCALAR_PROFILE setters=1687552 prepares=100000 set_us=%lld prepare_us=%lld checksum=%llu CPU_RENDER_STATE_ONLY_NOT_GPU\n",(long long)setter,(long long)prepare,(unsigned long long)hash);
}

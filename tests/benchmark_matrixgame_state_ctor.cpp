#ifdef NDEBUG
#error Keep benchmark assertions
#endif
#include "native_device_state.hpp"
#include "native_state_layout.hpp"
#include <chrono>
#include <cstdio>
#include <cassert>
namespace p=srhd_awa::matrixgame::platform3g;
void StencilProbe(const srhd_awa::matrixgame::MeshStencilState&);
int main(){p::NativeRenderState state;state.vertex.Layout(0x112);std::uint64_t hash=1469598103934665603ull;
 // Warm the same state/draw paths in both variants before measuring.
 for(const auto& w:p::NativeRenderWords)state.Render(w.id,w.initial);
 for(unsigned n=0;n<10000;++n)(void)state.PrepareDraw();
 for(unsigned n=0;n<1000;++n)for(const auto& w:p::NativeRenderWords)hash=(hash^state.Render(w.id))*1099511628211ull;
 srhd_awa::matrixgame::MeshStencilState stencil;for(unsigned n=0;n<10000;++n)StencilProbe(stencil);
 auto start=std::chrono::steady_clock::now();
 for(unsigned n=0;n<16384;++n)for(const auto& w:p::NativeRenderWords){state.Render(w.id,w.initial);hash=(hash^state.registers.at(w.id))*1099511628211ull;}
 auto setter=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();for(unsigned n=0;n<100000;++n){const auto draw=state.PrepareDraw();hash=(hash^draw.fog.controls[0])*1099511628211ull;}
 auto prepare=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();assert(hash);
 start=std::chrono::steady_clock::now();for(unsigned n=0;n<65536;++n)for(const auto& w:p::NativeRenderWords)hash=(hash^state.Render(w.id))*1099511628211ull;
 auto get=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();for(unsigned n=0;n<262144;++n)for(auto id:srhd_awa::matrixgame::RasterRegisters)hash=(hash^state.Render(unsigned(id)))*1099511628211ull;
 auto raster=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();assert(hash);
 start=std::chrono::steady_clock::now();for(unsigned n=0;n<4194304;++n){stencil.clockwise={(n&7)+1,((n>>3)&7)+1,((n>>6)&7)+1,((n>>9)&7)+1};stencil.counterclockwise={((n>>12)&7)+1,((n>>15)&7)+1,((n>>18)&7)+1,((n>>21)&7)+1};stencil.reference=n;StencilProbe(stencil);hash=(hash^stencil.reference)*1099511628211ull;}
 auto validate=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();assert(hash);
 for(unsigned n=0;n<2048;++n){p::NativeRenderState next;hash=(hash^next.registers.size())*1099511628211ull;}
 start=std::chrono::steady_clock::now();for(unsigned n=0;n<32768;++n){p::NativeRenderState next;hash=(hash^next.registers.size()^next.registers.begin()->second^next.registers.rbegin()->second)*1099511628211ull;}
 auto ctor=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();assert(hash);
 std::printf("CTOR_PROFILE constructors=32768 nodes_per_map=103 validators=4194304 setters=1687552 prepares=100000 getters=6750208 raster_getters=2883584 set_us=%lld get_us=%lld raster_us=%lld prepare_us=%lld validate_us=%lld ctor_us=%lld checksum=%llu CPU_ONLY_NOT_GPU\n",(long long)setter,(long long)get,(long long)raster,(long long)prepare,(long long)validate,(long long)ctor,(unsigned long long)hash);
}

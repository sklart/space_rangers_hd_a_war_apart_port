#include "native_command_state_cache.hpp"
#include <array>
#include <cassert>
#include <cstdio>
using srhd_awa::matrixgame::platform3g::NativeCommandStateCache;
int main(){
 NativeCommandStateCache cache;
 const std::array<float,4> a{.5f,.5f,1280,720},b{10.5f,.5f,100,80};
 assert(cache.NeedsVertexLayout() && cache.NeedsViewport(a));
 cache.VertexLayoutBound();cache.ViewportBound(a);
 for(unsigned i=0;i<1000;++i)assert(!cache.NeedsVertexLayout() && !cache.NeedsViewport(a));
 assert(cache.NeedsViewport(b));cache.ViewportBound(b);
 assert(!cache.NeedsViewport(b) && cache.NeedsViewport(a));
 cache.Invalidate(); // new command recording or external gamma renderer
 assert(cache.NeedsVertexLayout() && cache.NeedsViewport(b));
 cache.VertexLayoutBound();cache.ViewportBound(b);
 cache.Invalidate();assert(cache.NeedsVertexLayout() && cache.NeedsViewport(b));
 const std::array<unsigned,4> clip{0,0,1280,720};
 assert(cache.NeedsScissor(clip));cache.ScissorBound(clip);assert(!cache.NeedsScissor(clip));
 cache.InvalidateScissor();assert(cache.NeedsScissor(clip));
 assert(cache.NeedsMultisample());cache.MultisampleBound();assert(!cache.NeedsMultisample());
 cache.Invalidate();assert(cache.NeedsMultisample());
 assert(cache.NeedsMaterialBindings());cache.MaterialBindingsBound();assert(!cache.NeedsMaterialBindings());
 cache.Invalidate();assert(cache.NeedsMaterialBindings());
 const std::array<std::uint32_t,2> raster{1,3},other{2,3};
 assert(cache.NeedsRaster(raster));cache.RasterBound(raster);assert(!cache.NeedsRaster(raster) && cache.NeedsRaster(other));
 cache.Invalidate();assert(cache.NeedsRaster(raster));
 std::array<std::uint32_t,11> color{};assert(cache.NeedsColor(color));cache.ColorBound(color);assert(!cache.NeedsColor(color));
 for(unsigned i=0;i<color.size();++i){auto changed=color;changed[i]=1;assert(cache.NeedsColor(changed));}
 cache.Invalidate();assert(cache.NeedsColor(color));
 assert(cache.NeedsLineState());cache.LineStateBound();assert(!cache.NeedsLineState());
 cache.Invalidate();assert(cache.NeedsLineState());
 NativeCommandStateCache::DepthKey depth{true,true,4,{}};
 assert(cache.NeedsDepth(depth));cache.DepthBound(depth);assert(!cache.NeedsDepth(depth));
 auto different=depth;different.depth_test=false;assert(cache.NeedsDepth(different));
 different=depth;different.depth_write=false;assert(cache.NeedsDepth(different));
 different=depth;different.depth_compare=5;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.enabled=true;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.two_sided=true;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.clockwise.pass=2;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.counterclockwise.compare=2;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.reference=3;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.read_mask=7;assert(cache.NeedsDepth(different));
 different=depth;different.stencil.write_mask=7;assert(cache.NeedsDepth(different));
 cache.Invalidate();assert(cache.NeedsDepth(depth));
 // Replay changing viewports, recorder resets, and a foreign renderer. The
 // observed device state at every draw must match unconditional commands.
 std::array<float,4> gpu{};bool gpu_layout=false,gpu_material=false;std::array<std::uint32_t,2> gpu_raster{};
 NativeCommandStateCache::DepthKey gpu_depth{};
 unsigned viewport_emits=0,layout_emits=0,material_emits=0,raster_emits=0;
 for(unsigned i=0;i<4096;++i){
  if(i%97==0){cache.Invalidate();gpu={-1,-1,1,1};gpu_layout=false;gpu_material=false;gpu_raster={0,0};gpu_depth={};}
  if(i%113==0){cache.Invalidate();gpu={0,0,1,1};gpu_layout=false;gpu_material=false;gpu_raster={0,0};gpu_depth={};}
  const auto desired=(i/31)%2?a:b;
  if(cache.NeedsViewport(desired)){gpu=desired;cache.ViewportBound(desired);++viewport_emits;}
  if(cache.NeedsVertexLayout()){gpu_layout=true;cache.VertexLayoutBound();++layout_emits;}
  if(cache.NeedsMaterialBindings()){gpu_material=true;cache.MaterialBindingsBound();++material_emits;}
  const auto wanted=(i/19)%2?raster:other;
  if(cache.NeedsRaster(wanted)){gpu_raster=wanted;cache.RasterBound(wanted);++raster_emits;}
  auto wanted_depth=depth;if((i/23)%2)wanted_depth.stencil.reference=7;
  if(cache.NeedsDepth(wanted_depth)){gpu_depth=wanted_depth;cache.DepthBound(wanted_depth);}
  assert(gpu==desired && gpu_layout && gpu_material && gpu_raster==wanted && gpu_depth==wanted_depth);
 }
 assert(viewport_emits<4096 && layout_emits<4096 && material_emits<4096 && raster_emits<4096);
 std::puts("command-state cache reuse and recording/external invalidation PASS; GPU NOT RUN");
}

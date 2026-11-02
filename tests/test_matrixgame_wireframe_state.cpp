// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_device_state.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#ifdef MATRIXGAME_WIREFRAME_CANDIDATE
#include "legacy_polygon_mode.hpp"
#endif
namespace p=srhd_awa::matrixgame::platform3g;
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
 p::NativeStateStore store;
 if(argc==2 && !std::strcmp(argv[1],"--baseline")){
  store.Edit().Render(8,2);Reject([&]{store.Read().ValidateDraw();});
  std::puts("BASELINE original selection WIREFRAME draw refused: REPRODUCED");return 0;
 }
#ifdef MATRIXGAME_WIREFRAME_CANDIDATE
 for(unsigned fill:{2u,3u}){
  auto saved=store.Capture();store.Edit().Render(8,fill);store.Read().ValidateDraw();assert(store.Read().Render(8)==fill);
  dk::RasterizerState raster;raster.setCullMode(DkFace_Front).setDepthClampEnable(true);
  srhd_awa::matrixgame::ApplyLegacyPolygonMode(raster,fill);
  const auto expected=fill==2?DkPolygonMode_Line:DkPolygonMode_Fill;
  assert(raster.polygonModeFront==expected && raster.polygonModeBack==expected && raster.cullMode==DkFace_Front && raster.depthClampEnable);
  const auto before=raster;Reject([&]{srhd_awa::matrixgame::ApplyLegacyPolygonMode(raster,1);});
  assert(!std::memcmp(&before,&raster,sizeof(raster)));
  store.Edit().Render(8,fill==2?3:2);assert(saved->Restore()==0 && store.Read().Render(8)==3);
 }
 store.Edit().Render(8,2);store.Edit().Render(16,0);Reject([&]{store.Read().ValidateDraw();});store.Edit().Render(16,1);
 store.Edit().Render(176,1);Reject([&]{store.Read().ValidateDraw();});store.Edit().Render(176,0);store.Read().ValidateDraw();
 store.Edit().Render(8,1);Reject([&]{store.Read().ValidateDraw();});store.Edit().Render(8,2);
 Reject([&]{store.Read().ValidateDraw(true);});Reject([&]{store.Read().ValidateDraw(false,false,2);});
 store.Edit().Render(174,1);Reject([&]{store.Read().ValidateDraw();});store.Edit().Render(174,0);
 // Original DrawPass2/DrawPassEnd scalar sequence, with real stencil state.
 store.Edit().Render(52,1);store.Edit().Render(56,3);store.Edit().Render(55,2);store.Read().ValidateDraw();
 store.Edit().Render(8,3);store.Edit().Render(52,0);store.Read().ValidateDraw();assert(!store.Read().Stencil().enabled);
 assert(!store.Outstanding());
 std::puts("native wireframe/solid SDK rasterizer fields exact; capture/restore/stencil transition/AA-LASTPIXEL-point-MSAA-scissor guards PASS CPU_SDK_STATE_ONLY NO_GPU NO_SWITCH");
#endif
}

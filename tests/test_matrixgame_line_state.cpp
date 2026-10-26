// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_device_state.hpp"
#include "legacy_raster_viewport.hpp"
#include <cassert>
#include <cstdio>
namespace p=srhd_awa::matrixgame::platform3g;
namespace b=srhd_awa::matrixgame::buffers;
template<class F>void Unsupported(F f){bool rejected=false;try{f();}catch(const p::Unsupported&){rejected=true;}assert(rejected);}
int main(){
 const std::array<float,4> logical{13,442,145,145};
 assert((srhd_awa::matrixgame::LegacyRasterClip(logical)==std::array<unsigned,4>{13,442,145,145}));
 assert((srhd_awa::matrixgame::LegacyRasterViewport(logical)==std::array<float,4>{13.5f,442.5f,145,145}));
 b::Context buffers;b::Scope scope(buffers);p::NativeStateStore store;
 auto& initial=store.Edit();initial.vertex.Layout(4);initial.vertex.Viewport({13,442,145,145,0,1});
 initial.ValidateDraw(false,true);initial.Render(16,0);
 auto saved=store.Capture();store.Edit().Render(16,1);store.Read().ValidateDraw(false,true);
 assert(saved->Restore()==0 && store.Read().Render(16)==0);
 Unsupported([&]{store.Read().ValidateDraw(false,true);});
 store.Edit().Render(16,1);store.Edit().Render(176,1);
 Unsupported([&]{store.Read().ValidateDraw(false,true);});
 store.Edit().Render(176,0);store.Read().ValidateDraw(false,true);
 Unsupported([&]{store.Read().ValidateDraw(true,false);});
 // Supporting aliased lines must not bypass the existing scissor contract.
 store.Edit().Render(174,1);Unsupported([&]{store.Read().ValidateDraw(false,true);});
 store.Edit().Render(174,0);store.Read().ValidateDraw(false,true);saved.reset();assert(store.Outstanding()==0);
 std::puts("native line state PASS original LASTPIXEL=TRUE accepted; scalar/COW restore preserves FALSE and rejects unsupported draw; AA/point/scissor guards retained; CPU_ONLY NO_GPU NO_SWITCH_HARDWARE");
}

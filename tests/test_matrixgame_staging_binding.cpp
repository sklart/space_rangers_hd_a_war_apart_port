// Actual staging resources, fixed-vertex decode and native state leases. NO GPU.
#include "native_staging_binding.hpp"
#include "native_device_state.hpp"
#include "staging_draw.hpp"
#include "fixed_vertex.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace mg=srhd_awa::matrixgame;
namespace buffers=mg::buffers;
using namespace mg::platform3g;
#ifdef MATRIXGAME_BINDING_BASELINE
#include "r4-binding.inc"
namespace binding {auto BindVertex=OldVertex;auto BindIndex=OldIndex;}
#else
namespace binding=mg::native_staging;
#endif
template<class F>void Reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
struct Vertex {float x,y,z,u,v;};
int main(){
    buffers::Context context(4096),foreign(4096);buffers::Scope scope(context);
    auto* a=context.CreateVertex(80,0x102);auto* b=context.CreateVertex(80,0x102);
    auto* ia=context.CreateIndex(6,buffers::IndexWidth::U16);auto* ib=context.CreateIndex(6,buffers::IndexWidth::U16);
    assert(a && b && ia && ib);
    const std::array<Vertex,4> va{{{10,11,12,0,0},{13,14,15,0,1},{16,17,18,1,0},{19,20,21,1,1}}};
    auto vb=va;for(auto& v:vb)v.x+=100;
    std::memcpy(a->Map(0,0),va.data(),80);a->Unmap();std::memcpy(b->Map(0,0),vb.data(),80);b->Unmap();
    const std::array<std::uint16_t,3> indices{0,1,2};
    for(auto* i:{ia,ib}){std::memcpy(i->Map(0,0),indices.data(),6);i->Unmap();}
    // BigVB's direct selection precedes the pipeline's explicit water selection.
    context.BindVertex(a,0,sizeof(Vertex));context.BindIndex(ia);
    NativeStateStore state;auto before=state.Capture();
    assert(binding::BindVertex(0,b,sizeof(Vertex),sizeof(Vertex))==0);
    assert(binding::BindIndex(ib)==0);
    assert(context.Vertex().buffer==b && context.Vertex().offset==sizeof(Vertex) && context.Index()==ib);
    mg::materials::FixedVertexState decoder;decoder.Layout(0x102);
    auto draw=[&]{const auto v=context.Vertex();mg::staging_draw::Vertex(v,0);
        return decoder.DecodeOne(v.buffer->Storage().subspan(v.offset,v.stride),unsigned(v.stride));};
    assert(draw().position[0]==113);
    auto water=state.Capture();
    Reject([&]{binding::BindVertex(1,a,0,sizeof(Vertex));});
    Reject([&]{binding::BindVertex(0,a,0,0);});Reject([&]{binding::BindVertex(0,a,80,sizeof(Vertex));});
    a->Map(0,20);Reject([&]{binding::BindVertex(0,a,0,sizeof(Vertex));});a->Unmap();
    ia->Map(0,6);Reject([&]{binding::BindIndex(ia);});ia->Unmap();
    auto* fv=foreign.CreateVertex(80,0x102);auto* fi=foreign.CreateIndex(6,buffers::IndexWidth::U16);
    Reject([&]{binding::BindVertex(0,fv,0,sizeof(Vertex));});Reject([&]{binding::BindIndex(fi);});fv->Release();fi->Release();
    assert(context.Vertex().buffer==b && context.Index()==ib && draw().position[0]==113);
    // Leases and bindings keep resources alive after original owners release them.
    a->Release();b->Release();ia->Release();ib->Release();
    assert(before->Restore()==0 && context.Vertex().buffer==a && context.Index()==ia && draw().position[0]==10);
    assert(water->Restore()==0 && context.Vertex().buffer==b && context.Index()==ib && draw().position[0]==113);
    assert(binding::BindVertex(0,nullptr,17,23)==0 && binding::BindIndex(nullptr)==0);
    assert(!context.Vertex().buffer && context.Vertex().offset==0 && context.Vertex().stride==0 && !context.Index());
    before.reset();water.reset();assert(context.AllocatedBytes()==0 && foreign.AllocatedBytes()==0);
    std::puts("STAGING_BINDING PASS switch/offset/stride/null/index decode_actual_bytes snapshot_restore retain/release foreign/locked/stream/range_rejected CPU_ONLY NO_GPU");
}

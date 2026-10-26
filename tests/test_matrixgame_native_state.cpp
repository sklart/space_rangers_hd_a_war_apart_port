// SPDX-License-Identifier: GPL-2.0-or-later
// Executes the SAME state store used by DekoDevice. No emulated GPU/device.
#include "native_device_state.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
using namespace srhd_awa::matrixgame;
namespace p=platform3g;
static thread_local long fail_after=-1;
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto* ptr=std::malloc(n?n:1))return ptr;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* ptr)noexcept{std::free(ptr);}
void operator delete[](void* ptr)noexcept{std::free(ptr);}
void operator delete(void* ptr,std::size_t)noexcept{std::free(ptr);}
void operator delete[](void* ptr,std::size_t)noexcept{std::free(ptr);}
template<class F>void Reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}assert(bad);}
struct Observed {
    materials::Uniforms material;
    materials::VertexUniforms vertex;
    std::array<materials::Matrix,11> transforms;
    std::array<bool,8> bound;
    std::array<std::array<std::uint32_t,8>,8> samplers;
    std::array<const native_texture::Texture*,8> textures;
    std::array<std::uint32_t,11> raster;
    std::map<std::uint32_t,std::uint32_t> registers;
    std::uint32_t fvf,stride,cull,shade;
};
Observed Read(const p::NativeStateStore& store){const auto& s=store.Read();Observed o{};o.material=s.combiner.State();o.vertex=s.vertex.Prepare();
    for(unsigned i=0;i<11;++i)o.transforms[i]=s.vertex.Transform(i<3?(i==0?256:i==1?2:3):16+i-3);
    o.bound=s.combiner.Bound();
    for(unsigned i=0;i<8;++i){auto words=s.samplers[i].Words();static_assert(std::tuple_size_v<decltype(words)> == 8);o.samplers[i]=words;o.textures[i]=s.textures[i].get();}
    o.raster=s.raster.Words();o.registers=s.registers;o.fvf=s.vertex.Layout();o.stride=s.vertex.Stride();o.cull=s.cull;o.shade=s.shade;return o;}
void Same(const p::NativeStateStore& store,const Observed& expected){auto a=Read(store);assert(a.material.stages==expected.material.stages && a.material.factor==expected.material.factor);
    assert(!std::memcmp(&a.vertex,&expected.vertex,sizeof(a.vertex)));assert(a.samplers==expected.samplers && a.textures==expected.textures && a.raster==expected.raster && a.registers==expected.registers);
    assert(a.transforms==expected.transforms && a.bound==expected.bound);
    assert(a.fvf==expected.fvf && a.stride==expected.stride && a.cull==expected.cull && a.shade==expected.shade);}
void Configure(p::NativeStateStore& store){
    auto& s=store.Edit();s.vertex.Layout(0x852);s.vertex.Viewport({7,9,640,480,0,1});
    for(unsigned i=0;i<11;++i){auto m=materials::Identity();m[0]=1.f+i*.1f;m[5]=2.f+i*.1f;m[10]=3.f+i*.1f;m[12]=float(i);
        s.vertex.Transform(i<3?(i==0?256:i==1?2:3):16+i-3,m);}
    p::VideoMaterial material{};material.Diffuse={.3f,.4f,.5f,.6f};material.Ambient={.1f,.2f,.3f,.4f};material.Emissive={.2f,.3f,.4f,.5f};s.vertex.Material(material);
    for(unsigned i=0;i<8;++i){p::VideoLight l{};l.Type=3;l.Direction={1.f+float(i),-2.f,3.f};l.Diffuse={.7f,.6f,.5f,.4f};l.Ambient={.1f,.2f,.3f,1};s.vertex.Light(i,l);s.vertex.EnableLight(i,i%2==0);
        s.vertex.Stage(i,11,(i%4)*0x10000u+i);s.vertex.Stage(i,24,2);
        s.samplers[i].Apply(SamplerRegister::MaxAnisotropy,i+1);s.samplers[i].Apply(SamplerRegister::AddressU,i%2?3:1);
        s.combiner.Set(i,1,2);s.combiner.Set(i,2,1);s.combiner.Set(i,4,1);s.combiner.Set(i,28,i%2?5:1);
    }
    s.vertex.Render(137,1);s.vertex.Render(143,1);s.vertex.Render(139,0xff314159);s.combiner.Factor(0x813763a1);
    const std::array<std::uint32_t,11> words{0,0,1,2,1,5,173,3,1,7,3};
    for(unsigned i=0;i<words.size();++i){s.raster.Apply(RasterRegisters[i],words[i]);s.registers[unsigned(RasterRegisters[i])]=words[i];}
    s.cull=1;
}
void ContextLifetimes(){
    p::NativeStateStore store;auto empty=store.Capture();assert(empty->Restore()==0);
    auto context=std::make_unique<buffers::Context>();std::unique_ptr<p::DeviceStateLease> lease;buffers::BindingSnapshot retained;
    {buffers::Scope scope(*context);assert(empty->Restore()!=0);
        auto* vertex=context->CreateVertex(48,2);auto* index=context->CreateIndex(6,buffers::IndexWidth::U16);
        context->BindVertex(vertex,0,12);context->BindIndex(index);vertex->Release();index->Release();
        retained=context->CaptureBindings();lease=store.Capture();store.Edit().cull=2;}
    {buffers::Context foreign;buffers::Scope scope(foreign);fail_after=0;assert(lease->Restore()!=0);fail_after=-1;assert(!foreign.Vertex().buffer && !foreign.Index() && store.Read().cull==2);}
    assert(lease->Restore()!=0 && store.Read().cull==2);context.reset();
    assert(retained.Vertex().buffer->Size()==48 && retained.Index()->Size()==6);
    assert(!retained.CanRestore() && !retained.Restore() && lease->Restore()!=0 && store.Read().cull==2);
    lease.reset();retained={};assert(empty->Restore()==0);empty.reset();assert(store.Outstanding()==0);
}
int main(){
    ContextLifetimes();
    unsigned restores=0,failures=0;
    p::NativeStateStore store;buffers::Context context(4096);buffers::Scope scope(context);Configure(store);
    auto empty=store.Capture();auto* vb=context.CreateVertex(96,2);auto* ib=context.CreateIndex(12,buffers::IndexWidth::U16);assert(vb && ib);
    std::memset(vb->Map(0,0),0x51,96);vb->Unmap();std::memset(ib->Map(0,0),0x73,12);ib->Unmap();context.BindVertex(vb,12,12);context.BindIndex(ib);
    assert(empty->Restore()==0 && !context.Vertex().buffer && !context.Index());++restores;empty.reset();context.BindVertex(vb,12,12);context.BindIndex(ib);
    auto texture=std::make_shared<native_texture::Texture>();texture->levels.push_back({1,1,4,1,std::vector<std::byte>(4,std::byte{0x47})});std::weak_ptr<native_texture::Texture> texture_lifetime=texture;
    store.Edit().textures[3]=texture;store.Edit().combiner.Bind(3,true);texture.reset();
    auto before=Read(store);auto lease=store.Capture();assert(store.Outstanding()==1);
    // Mutation of caller-owned resource bytes must not be reverted by state restoration.
    std::memset(vb->Map(0,12),0x92,12);vb->Unmap();vb->Release();ib->Release();
    context.ClearBindings();store.Edit()=p::NativeRenderState{};
    assert(!texture_lifetime.expired() && context.AllocatedBytes()==108);
    fail_after=0;assert(lease->Restore()==0);fail_after=-1;++restores;Same(store,before);
    assert(context.Vertex().buffer==vb && context.Vertex().offset==12 && context.Vertex().stride==12 && context.Index()==ib);
    assert(vb->Storage()[0]==std::byte{0x92});
    for(unsigned i=0;i<3;++i){store.Edit().vertex.Transform(256,materials::Identity());context.ClearBindings();fail_after=0;assert(lease->Restore()==0);fail_after=-1;++restores;Same(store,before);}
    // Capturing must never upload/read resource contents; locked bindings are state too.
    vb->Map(0,4);auto locked=store.Capture();assert(locked->Restore()==0);++restores;vb->Unmap();locked.reset();assert(store.Outstanding()==1);
    auto nested=store.Capture();store.Edit().cull=2;auto second_state=Read(store);auto second=store.Capture();store.Edit().cull=3;
    assert(nested->Restore()==0);++restores;Same(store,before);assert(second->Restore()==0);++restores;Same(store,second_state);
    nested.reset();second.reset();assert(store.Outstanding()==1);
    for(long n=0;n<4;++n){const auto count=store.Outstanding();fail_after=n;
        try{auto test=store.Capture();fail_after=-1;assert(store.Outstanding()==count+1);}catch(const std::bad_alloc&){fail_after=-1;++failures;}
        assert(store.Outstanding()==count);Same(store,second_state);assert(context.AllocatedBytes()==108);}
    auto cow=store.Capture();fail_after=0;bool oom=false;try{store.Edit().cull=1;}catch(const std::bad_alloc&){oom=true;}fail_after=-1;assert(oom);Same(store,second_state);cow.reset();
    store.Frame(true);Reject([&]{store.Capture();});assert(lease->Restore()!=0);Same(store,second_state);store.Frame(false);assert(lease->Restore()==0);++restores;
    lease.reset();assert(store.Outstanding()==0);
    // A destroyed snapshot does not implicitly Apply. Close invalidates leases
    // safely even when those leases outlive their device/state store.
    std::unique_ptr<p::DeviceStateLease> late;
    {p::NativeStateStore temporary;temporary.Edit().cull=2;late=temporary.Capture();temporary.Edit().cull=3;assert(temporary.Outstanding()==1);}
    fail_after=0;assert(late->Restore()!=0);fail_after=-1;late.reset();
    // Failure on a missing/foreign/dead context cannot partially restore state.
    std::unique_ptr<p::DeviceStateLease> empty_owner;{p::NativeStateStore clean;empty_owner=clean.Capture();}assert(empty_owner->Restore()!=0);empty_owner.reset();
    {auto dead=std::make_unique<buffers::Context>();buffers::BindingSnapshot saved;
        // Existing scope cannot be displaced by a foreign context.
        saved=dead->CaptureBindings();assert(!saved.CanRestore());dead.reset();assert(!saved.CanRestore() && !saved.Restore());}
    before.textures={};second_state.textures={};store.Edit().textures={};context.ClearBindings();assert(context.AllocatedBytes()==0 && texture_lifetime.expired());
    assert(failures==2);
    std::printf("native state CPU PASS restores=%u capture_OOM=%u COW_OOM=1 restore_allocations=0 held_texture_buffer=preserved frame_guard=checked expired_owner=checked GPU_NOT_RUN\n",restores,failures);
}

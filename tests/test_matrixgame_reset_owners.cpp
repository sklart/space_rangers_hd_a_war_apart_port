// SPDX-License-Identifier: GPL-2.0-or-later
// Production native ownership predicate + reset transaction. GPU/VI allocation
// is explicitly a CPU fixture; this does not execute native Init or graphics.
#include "native_reset_resources.hpp"
#include "native_presentation.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
namespace {bool reject_allocation{};}
void* operator new(std::size_t n){if(reject_allocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace m=srhd_awa::matrixgame;namespace p=m::platform3g;namespace nt=m::native_texture;namespace ns=m::native_surfaces;namespace pr=p::presentation;
struct CpuImage final:nt::Image{};
struct SurfaceImage final:ns::Resource {
    std::shared_ptr<nt::Texture> texture;
    const std::shared_ptr<nt::Texture>* ResetTextureReference()const noexcept override{return &texture;}
};
struct Fixture final:pr::Driver {
    struct Target final:pr::Targets {
        std::shared_ptr<p::NativeRenderState> defaults=std::make_shared<p::NativeRenderState>();
        std::shared_ptr<ns::Surface> color,depth;
        Target(const p::Presentation& value):Targets(value){}
    };
    p::NativeStateStore state;
    std::shared_ptr<nt::Session> textures=std::make_shared<nt::Session>();
    std::shared_ptr<ns::Session> surfaces=std::make_shared<ns::Session>();
    bool healthy=true,busy=false,oom{};unsigned allocations{},commits{},attach_failures{};pr::Targets* attached{};
    Fixture(){
        textures->upload=[](const nt::Texture&){return std::make_shared<CpuImage>();};surfaces->textures=textures;
        surfaces->viewport=[](const p::VideoViewport&){};
        surfaces->allocate=[](const m::offscreen::SurfaceDesc&,bool){return std::make_shared<SurfaceImage>();};
        surfaces->import_texture=[](const std::shared_ptr<nt::Texture>& texture,unsigned){auto resource=std::make_shared<SurfaceImage>();resource->texture=texture;return resource;};
    }
    bool Healthy()const noexcept override{return healthy;}bool Busy()const noexcept override{return busy;}
    bool DefaultResourcesLive()const noexcept override{return p::DefaultResourcesBlockReset(state,textures.get(),surfaces.get());}
    std::unique_ptr<pr::Targets> Allocate(const p::Presentation& value)override{
        ++allocations;if(oom)throw std::bad_alloc();auto target=std::make_unique<Target>(value);
        target->color=surfaces->Make({21,0,0,value.BackBufferWidth,value.BackBufferHeight},false,false,true,std::make_shared<SurfaceImage>());
        target->depth=surfaces->Make({75,0,0,value.BackBufferWidth,value.BackBufferHeight},true,false,true,std::make_shared<SurfaceImage>());
        return target;
    }
    void Quiesce()override{assert(!busy);}
    void Attach(pr::Targets& target)override{assert(!attached);attached=&target;if(attach_failures){--attach_failures;throw std::bad_alloc();}}
    void Detach(pr::Targets&)override{attached=nullptr;}
    void Commit(pr::Targets& target)noexcept override{
        auto& next=static_cast<Target&>(target);state.ResetPrepared(std::move(next.defaults));surfaces->Presentation(std::move(next.color),std::move(next.depth));++commits;
    }
    IDirect3DTexture9* Texture(unsigned pool=0,unsigned usage=0){IDirect3DTexture9* token{};nt::Create(textures,16,16,1,usage,21,pool,&token);return token;}
};
int main(){
    Fixture driver;pr::Transaction transaction(driver);p::Presentation presentation{};
    presentation.BackBufferWidth=128;presentation.BackBufferHeight=128;presentation.BackBufferCount=2;
    assert(transaction.Reset(presentation)==0 && !driver.DefaultResourcesLive());
    auto changed=presentation;changed.BackBufferWidth=256;
    auto bound=driver.Texture();driver.state.Edit().textures[0]=nt::Find(bound);driver.state.Edit().textures[3]=nt::Find(bound);
    assert(driver.DefaultResourcesLive());nt::Release(bound);assert(driver.textures->DefaultResourcesLive());assert(!driver.DefaultResourcesLive());
    const auto* address=driver.state.Read().textures[0].get();std::weak_ptr<nt::Texture> weak=driver.state.Read().textures[0];
    reject_allocation=true;assert(!driver.DefaultResourcesLive());reject_allocation=false;
    driver.oom=true;assert(transaction.Reset(changed)==pr::OutOfMemory && driver.state.Read().textures[0].get()==address && !weak.expired());driver.oom=false;
    driver.attach_failures=1;assert(transaction.Reset(changed)==pr::OutOfMemory && driver.state.Read().textures[0].get()==address && !weak.expired());
    auto snapshot=driver.state.Capture();assert(driver.DefaultResourcesLive());const auto before=driver.allocations;
    assert(transaction.Reset(changed)==pr::InvalidCall && driver.allocations==before);
    driver.state.Edit().Render(24,13);assert(driver.DefaultResourcesLive()); // Snapshot is now a distinct COW state.
    snapshot.reset();assert(!driver.DefaultResourcesLive());assert(transaction.Reset(changed)==0 && weak.expired());
    {
        auto token=driver.Texture();auto owner=nt::Find(token);driver.state.Edit().textures[0]=owner;nt::Release(token);
        assert(driver.DefaultResourcesLive());owner.reset();assert(!driver.DefaultResourcesLive());assert(transaction.Reset(presentation)==0);
    }
    {
        auto token=driver.Texture();auto unrelated=std::make_shared<int>(0);
        std::shared_ptr<nt::Texture> alias(unrelated,nt::Find(token).get());driver.state.Edit().textures[0]=alias;
        assert(driver.DefaultResourcesLive()); // Equal raw address is not equal shared ownership.
        driver.state.Edit().textures[0].reset();nt::Release(token);
    }
    {
        nt::DeviceReferences references;std::array<std::shared_ptr<nt::Texture>,17> fields;
        for(auto& field:fields){field=std::make_shared<nt::Texture>();references.Add(field);}
        assert(references.Denied() && driver.textures->ExternalDefaultResourcesLive(references));
    }
    {
        auto target=driver.surfaces->Create({21,0,0,16,16},false,false);driver.surfaces->Color(target);
        assert(driver.DefaultResourcesLive());target.reset();assert(!driver.DefaultResourcesLive());
        auto lease=driver.surfaces->Capture();assert(driver.DefaultResourcesLive());lease.reset();assert(!driver.DefaultResourcesLive());
        assert(transaction.Reset(changed)==0);
    }
    {
        auto depth=driver.surfaces->Create({75,0,0,128,128},true,false);driver.surfaces->Depth(depth);depth.reset();assert(!driver.DefaultResourcesLive());
        assert(transaction.Reset(presentation)==0);
    }
    {
        auto token=driver.Texture(0,1);auto surface=driver.surfaces->Import(nt::Find(token),0);driver.surfaces->Color(surface);
        driver.state.Edit().textures[1]=nt::Find(token);driver.state.Edit().textures[4]=nt::Find(token);nt::Release(token);
        assert(driver.DefaultResourcesLive());surface.reset();assert(!driver.DefaultResourcesLive());
        // The same resource field can occur through multiple surface aliases;
        // count actual shared_ptr fields once, never owner.use_count guesses.
        nt::DeviceReferences references;const auto& field=driver.state.Read().textures[1];references.Add(field);references.Add(field);assert(references.Count(field)==1);
        auto resource=driver.surfaces->color->resource;assert(driver.DefaultResourcesLive());resource.reset();
        IDirect3DSurface9* handle{};ns::Expose(driver.surfaces->color,&handle);assert(driver.DefaultResourcesLive());ns::Release(handle);assert(!driver.DefaultResourcesLive());
        assert(transaction.Reset(changed)==0);
    }
    {
        auto back=driver.surfaces->primary_color;assert(driver.DefaultResourcesLive());back.reset();assert(!driver.DefaultResourcesLive());
        auto managed=driver.Texture(1);auto held=nt::Find(managed);driver.state.Edit().textures[0]=held;auto image=held->image;
        assert(!driver.DefaultResourcesLive());assert(transaction.Reset(presentation)==0 && held->image==image);nt::Release(managed);
    }
    driver.busy=true;const auto before_busy=driver.allocations;assert(transaction.Reset(changed)==pr::InvalidCall && driver.allocations==before_busy);driver.busy=false;
    driver.healthy=false;assert(transaction.Reset(changed)==pr::DeviceLost);driver.healthy=true;
    assert(nt::HandleCount()==0 && ns::HandleCount()==0 && !driver.DefaultResourcesLive());transaction.Release();driver.surfaces->Close();driver.textures->Shutdown();
    std::puts("production Reset ownership PASS device-only bindings released on commit; external handles/owners/COW snapshots/surface leases block; no-allocation guard; OOM/attach rollback exact; managed payload preserved; CPU_FIXTURES NO_NATIVE_INIT NO_GPU NO_SWITCH_HARDWARE");
}

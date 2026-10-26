// Execute the production reset coordinator and state/restore lease. Low-level
// GPU/VI/NWindow operations are a fault-injected CPU model, never a GPU PASS.
#include "native_presentation.hpp"
#include "native_device_state.hpp"
#include "native_frame_scope.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
namespace n=p::presentation;
struct Driver final:n::Driver {
    struct Resource final:n::Targets {Driver& driver;unsigned id;std::shared_ptr<p::NativeRenderState> state;
        Resource(Driver& d,const p::Presentation& v):Targets(v),driver(d),id(++d.serial),state(std::make_shared<p::NativeRenderState>()){
            state->vertex.Viewport({0,0,v.BackBufferWidth,v.BackBufferHeight,0,1});++driver.live;}
        ~Resource(){assert(driver.attached!=this);--driver.live;}};
    bool healthy=true,busy=false,defaults=false,fail_allocation=false,fail_quiesce=false,fail_detach=false;
    unsigned attach_failures{},serial{},live{},commits{},idle{};Resource* attached{};p::NativeStateStore state;
    std::vector<char> calls;
    bool Healthy()const noexcept override{return healthy;}
    bool Busy()const noexcept override{return busy;}
    bool DefaultResourcesLive()const noexcept override{return defaults;}
    std::unique_ptr<n::Targets> Allocate(const p::Presentation& v)override{calls.push_back('A');if(fail_allocation)throw std::bad_alloc();return std::make_unique<Resource>(*this,v);}
    void Quiesce()override{calls.push_back('Q');if(fail_quiesce)throw std::runtime_error("injected queue completion failure");++idle;}
    void Detach(n::Targets&)override{calls.push_back('D');attached=nullptr;if(fail_detach){fail_detach=false;throw std::runtime_error("injected unregister failure");}}
    void Attach(n::Targets& value)override{calls.push_back('T');assert(!attached);attached=&static_cast<Resource&>(value);if(attach_failures){--attach_failures;throw std::runtime_error("injected partial registration failure");}}
    void Commit(n::Targets& value)noexcept override{++commits;state.ResetPrepared(std::move(static_cast<Resource&>(value).state));}
};
int main(){
    int identity{};p::Presentation input{};input.BackBufferWidth=1280;input.BackBufferHeight=720;input.BackBufferFormat=p::PixelFormat::Argb8;
    input.hDeviceWindow=&identity;input.Windowed=true;input.SwapEffect=1;input.EnableAutoDepthStencil=true;input.AutoDepthStencilFormat=p::PixelFormat::Depth24Stencil8;input.PresentationInterval=1;input.Flags=2;
    auto baseline=n::Plan(input,&identity,60);assert(baseline.BackBufferCount==2);
    auto immediate=input;immediate.PresentationInterval=0x80000000u;immediate=n::Plan(immediate,&identity,60);assert(immediate.BackBufferCount==3);
    for(unsigned samples=0;samples<=16;++samples)assert(n::Multisample(0,1,p::PixelFormat::Argb8,true,samples)==(samples?n::NotAvailable:0));
    assert(n::Multisample(1,1,p::PixelFormat::Argb8,true,0)==n::InvalidCall);assert(n::Multisample(0,2,p::PixelFormat::Argb8,true,0)==n::InvalidCall);
    const auto reject=[](auto call){bool caught=false;try{call();}catch(const std::exception&){caught=true;}assert(caught);};
    for(unsigned kind=0;kind<11;++kind){auto bad=input;switch(kind){case 0:bad.BackBufferWidth=0;break;case 1:bad.BackBufferHeight=8193;break;case 2:bad.hDeviceWindow=nullptr;break;
        case 3:bad.MultiSampleType=4;break;case 4:bad.MultiSampleQuality=1;break;case 5:bad.BackBufferCount=4;break;case 6:bad.AutoDepthStencilFormat=p::PixelFormat::Argb8;break;
        case 7:bad.BackBufferFormat=p::PixelFormat::Depth16;break;case 8:bad.PresentationInterval=2;break;case 9:bad.FullScreen_RefreshRateInHz=75;break;case 10:bad.EnableAutoDepthStencil=false;}
        reject([&]{n::Plan(bad,&identity,60);});}
    for(auto format:{p::PixelFormat::Argb8,p::PixelFormat::Xrgb8,p::PixelFormat::Rgb565})for(auto depth:{p::PixelFormat::Depth24Stencil8,p::PixelFormat::Depth24,p::PixelFormat::Depth16}){
        auto valid=input;valid.BackBufferFormat=format;valid.AutoDepthStencilFormat=depth;assert(n::Plan(valid,&identity,60).BackBufferFormat==format);}
    std::array<std::uint64_t,9> stamps{};for(unsigned i=0;i<9;++i)stamps[i]=1'000'000'000ull+i*16'666'667ull;assert(n::RefreshFromVblanks(stamps)==60);
    stamps[8]+=16'666'667;reject([&]{n::RefreshFromVblanks(stamps);});stamps[1]=0;reject([&]{n::RefreshFromVblanks(stamps);});reject([&]{n::RefreshFromVblanks(std::span(stamps).first(8));});
    {
        // Initial device state must not unbind a borrowing host's context.
        srhd_awa::matrixgame::buffers::Context context;srhd_awa::matrixgame::buffers::Scope scope(context);
        auto* buffer=context.CreateVertex(12,2);assert(buffer);context.BindVertex(buffer,0,12);
        p::NativeStateStore state;state.ResetPrepared(std::make_shared<p::NativeRenderState>(),false);assert(context.Vertex().buffer==buffer);
        auto lease=state.Capture();state.ResetPrepared(std::make_shared<p::NativeRenderState>());assert(!context.Vertex().buffer);
        assert(lease->Restore()==0 && context.Vertex().buffer==buffer);lease.reset();context.ClearBindings();buffer->Release();assert(!context.AllocatedBytes());
    }
    {
        p::NativeFrameScope frame;reject([&]{frame.RequireClear();});frame.Acquire();frame.RequireClear();frame.BeginScene();frame.RequireClear();
        frame.EndScene();frame.Submitted();reject([&]{frame.RequireClear();});frame.Abort();assert(!frame);
    }
    {
        // A default resource held by another owner still blocks Reset, while
        // managed CPU/image payloads survive target recreation.
        srhd_awa::matrixgame::native_texture::Session session;
        auto texture=std::make_shared<srhd_awa::matrixgame::native_texture::Texture>();texture->pool=0;session.textures.push_back(texture);
        auto held=texture;texture.reset();assert(session.DefaultResourcesLive());held.reset();assert(!session.DefaultResourcesLive());
        auto managed=std::make_shared<srhd_awa::matrixgame::native_texture::Texture>();managed->pool=1;session.textures.push_back(managed);assert(!session.DefaultResourcesLive());
    }
    Driver driver;n::Transaction transaction(driver);assert(!transaction.Ready());assert(transaction.Reset(input)==n::InvalidCall && driver.calls.empty());assert(transaction.Reset(baseline)==0 && transaction.Ready() && driver.live==1);
    auto changed=baseline;changed.BackBufferWidth=800;changed.BackBufferHeight=600;changed= n::Plan(changed,&identity,60);
    const auto old=driver.attached->id;
    for(unsigned failure=0;failure<6;++failure){driver.calls.clear();const auto before=driver.commits;
        driver.fail_allocation=failure==0;driver.fail_quiesce=failure==1;driver.fail_detach=failure==2;driver.attach_failures=failure==3?1:0;driver.busy=failure==4;driver.defaults=failure==5;
        const auto result=transaction.Reset(changed);assert(result!=0 && transaction.Ready() && driver.attached->id==old && driver.live==1 && driver.commits==before);
        if(failure==0)assert(result==n::OutOfMemory && driver.calls==std::vector<char>({'A'}));
        if(failure==1)assert(driver.calls==std::vector<char>({'A','Q'}));
        if(failure==3)assert(driver.calls==std::vector<char>({'A','Q','D','T','D','T'}));
        if(failure>=4)assert(result==n::InvalidCall && driver.calls.empty());
        driver.fail_allocation=driver.fail_quiesce=driver.busy=driver.defaults=false;
    }
    assert(transaction.Reset(changed)==0 && driver.live==1 && driver.attached->id!=old);assert(driver.state.Read().vertex.Viewport().Width==800);
    driver.attach_failures=2;assert(transaction.Reset(baseline)==n::DeviceLost && !transaction.Ready() && driver.live==1);
    driver.attach_failures=0;assert(transaction.Reset(baseline)==0 && transaction.Ready() && driver.live==1);
    auto lifetime=std::make_shared<n::Lifetime>(n::Lifetime{&transaction});
    {
        auto snapshot=std::make_unique<n::RestoreLease>(lifetime,baseline,driver.state.Capture());assert(driver.state.Outstanding()==1);
        assert(transaction.Reset(changed)==0 && driver.state.Read().vertex.Viewport().Width==800);
        driver.fail_allocation=true;assert(snapshot->Restore()==n::OutOfMemory && driver.state.Read().vertex.Viewport().Width==800);
        driver.fail_allocation=false;assert(snapshot->Restore()==0 && driver.state.Read().vertex.Viewport().Width==1280);
        driver.busy=true;assert(snapshot->Restore()==n::InvalidCall);driver.busy=false;assert(snapshot->Restore()==0);
        lifetime->owner=nullptr;assert(snapshot->Restore()==n::DeviceLost);lifetime->owner=&transaction;
    }
    assert(driver.state.Outstanding()==0);driver.healthy=false;assert(transaction.Reset(changed)==n::DeviceLost);driver.healthy=true;
    transaction.Release();assert(driver.live==0 && !driver.attached && !transaction.Ready());
    std::puts("production native presentation Plan/Reset/rollback/retry/lease/default-resource refusal/VBlank checks PASS assertions=ON CPU_DRIVER_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN");
}

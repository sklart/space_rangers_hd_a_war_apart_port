// CPU execution of the actual viewport/store/query contracts; no GPU success claim.
#include <cassert>
#include <cstdio>
#include "native_map_queries.hpp"
#include "native_device_state.hpp"
#include "map_device_backend.hpp"
#include "offscreen_backend.hpp"
#include "transition_backend.hpp"
#include "native_frame_scope.hpp"
namespace p=srhd_awa::matrixgame::platform3g;
namespace map=srhd_awa::matrixgame::map_device;
namespace off=srhd_awa::matrixgame::offscreen;
struct Fixture final:p::Device,p::NativeMapQueries,p::NativeFrameProvider{
    p::NativeStateStore state;p::NativeFrameScope frame;bool queue_error{};
    unsigned clears{},presents{};std::vector<p::ClearRectangle> last_clear;
    p::DeviceResult NativeClear(std::span<const p::ClearRectangle> rectangles,unsigned flags,unsigned,float depth,unsigned stencil)override{
        if(!frame)throw std::logic_error("CPU model has no acquired frame");
        last_clear=p::PlanNativeClear(NativeReadViewport(),1280,720,rectangles,flags,depth,stencil);++clears;return 0;}
    p::DeviceResult NativeBeginScene()override{frame.BeginScene();return 0;}
    p::DeviceResult NativeEndScene()override{frame.EndScene();return 0;}
    p::DeviceResult NativePresent()override{frame.Submitted();frame.Presented();++presents;return 0;}
    p::VideoViewport NativeReadViewport()const override{return state.Read().vertex.Viewport();}
    p::DeviceResult NativeCooperativeLevel()const override{return queue_error?static_cast<p::DeviceResult>(0x88760868u):0;}
    p::DeviceResult SetVideoViewport(const p::VideoViewport& v)override{state.Edit().vertex.Viewport(v);return 0;}
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("CPU queries fixture has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("CPU queries fixture has no GPU budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("CPU queries fixture has no gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
int main(){Fixture device;assert(!p::Current().ready);p::Current().ready=true;p::Current().device=&device;
    p::VideoViewport a{17,29,1024,512,0,1},actual{};assert(off::SetViewportValue(a)==0 && off::GetViewport(&actual)==0);
    assert(actual.X==17 && actual.Y==29 && actual.Width==1024 && actual.Height==512 && actual.MinZ==0 && actual.MaxZ==1);
    auto lease=device.state.Capture();assert(off::SetViewportValue({0,0,128,64,0,1})==0);assert(lease->Restore()==0);lease.reset();off::GetViewport(&actual);assert(actual.X==17 && actual.Width==1024);
    for(auto bad:{p::VideoViewport{0,0,0,1,0,1},p::VideoViewport{0,0,16385,1,0,1},p::VideoViewport{0xffffffffu,0,1,1,0,1},p::VideoViewport{0,0,1,1,.5f,1}}){bool rejected=false;try{off::SetViewportValue(bad);}catch(const std::exception&){rejected=true;}assert(rejected);off::GetViewport(&actual);assert(actual.X==17 && actual.Width==1024);}
    assert(map::CooperativeLevel()==0);device.queue_error=true;assert(map::CooperativeLevel()==static_cast<p::DeviceResult>(0x88760868u));
    const auto rejected=[](auto call){bool bad=false;try{call();}catch(const std::exception&){bad=true;}assert(bad);};
    rejected([&]{off::BeginScene();});rejected([&]{srhd_awa::matrixgame::transition::Present();});
    device.frame.Acquire();off::Clear(0,nullptr,7,0xff123456,1,0);assert(device.clears==1 && device.last_clear.size()==1 && device.last_clear[0]==p::ClearRectangle({17,29,1041,541}));
    p::ClearRectangle rectangles[]{{-5,-7,32,48},{1000,500,1200,700},{-8,-8,-2,-2}};
    off::Clear(3,rectangles,2,0,0,0);assert(device.last_clear.size()==2 && device.last_clear[0]==p::ClearRectangle({17,29,32,48}) && device.last_clear[1]==p::ClearRectangle({1000,500,1041,541}));
    const auto clears=device.clears;rejected([&]{off::Clear(1,nullptr,1,0,1,0);});rejected([&]{off::Clear(0,nullptr,0,0,1,0);});rejected([&]{off::Clear(0,nullptr,2,0,-1,0);});assert(device.clears==clears);
    off::BeginScene();device.frame.RequireDraw();rejected([&]{off::BeginScene();});rejected([&]{srhd_awa::matrixgame::transition::Present();});off::EndScene();rejected([&]{off::EndScene();});
    srhd_awa::matrixgame::transition::Present();assert(device.presents==1 && !device.frame);rejected([&]{srhd_awa::matrixgame::transition::Present();});
    device.frame.Acquire();device.frame.BeginScene();device.frame.Abort();assert(!device.frame);rejected([&]{device.frame.EndScene();});
    device.frame.Acquire();device.frame.Submitted();rejected([&]{device.frame.RequirePresent();});rejected([&]{device.frame.BeginScene();});device.frame.Abort();assert(!device.frame);
    p::Current().ready=false;p::Current().device=nullptr;
    std::puts("native viewport/state/status + original Clear/BeginScene/EndScene/Present routing/clipping/double-present rejection PASS CPU_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN");
}

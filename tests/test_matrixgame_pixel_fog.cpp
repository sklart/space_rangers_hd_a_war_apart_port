// Production fog/native state and ORIGINAL CDeviceState; no GPU model or Init.
#include "native_stored_state.hpp"
#include "native_state_layout.hpp"
#include "native_map_queries.hpp"
#include "map_device_backend.hpp"
#include "DeviceState.hpp"
#include "legacy_raster_viewport.hpp"
#include <cassert>
#include <bit>
#include <cstdio>
#include <cstring>
#include <fstream>
using namespace srhd_awa::matrixgame;
namespace p=platform3g;namespace mat=materials;
template<class F>void Reject(F&& f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
struct Routing final:p::Device,p::NativeMapQueries,p::NativeStoredStateBackend {
    p::NativeStateStore state;mutable unsigned queries{};
    p::NativeStateStore& StoredStates()override{return state;}
    p::VideoViewport NativeReadViewport()const override{return state.Read().vertex.Viewport();}
    p::DeviceResult NativeCooperativeLevel()const override{throw p::Unsupported("fog CPU test has no GPU queue");}
    std::uint32_t NativeRasterCaps()const override{++queries;return mat::PixelFogCaps;}
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("fog CPU test has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("fog CPU test has no GPU budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("fog CPU test has no VI gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
struct Golden {unsigned mode,w_depth,enabled,source,fog;float start,end,density,depth;unsigned result;};
static_assert(sizeof(Golden)==40);
mat::Color Argb(unsigned c){return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255};}
int main(int argc,char** argv){assert(argc==2);std::ifstream input(argv[1],std::ios::binary);assert(input);
    unsigned count=0,max_delta=0;Golden g{};
    const mat::Matrix perspective{1,0,0,0,0,1,0,0,0,0,1.001f,1,0,0,-.1001f,0};
    while(input.read(reinterpret_cast<char*>(&g),sizeof(g))){auto matrix=g.w_depth?perspective:mat::Identity();
        const auto fog=mat::PreparePixelFog(g.enabled,g.mode,g.start,g.end,g.density,g.fog,matrix);
        const auto c=mat::ApplyPixelFog(fog,Argb(g.source),g.depth);const auto expected=Argb(g.result);
        for(unsigned i=0;i<4;++i){const auto delta=unsigned(std::lround(std::abs(c[i]-expected[i])*255));max_delta=std::max(max_delta,delta);assert(delta<=1);}
        assert(std::lround(c[3]*255)==int(g.source>>24));++count;
    }
    assert(input.eof() && input.gcount()==0 && count==840);
    Routing d;p::Current().device=&d;p::Current().ready=true;
    assert(map_device::RasterCaps()==mat::PixelFogCaps && d.queries==1);
    auto& s=d.state.Edit();s.Render(35,3);s.Render(36,std::bit_cast<unsigned>(2000.f));s.Render(37,std::bit_cast<unsigned>(2800.f));s.Render(38,std::bit_cast<unsigned>(.5f));s.Render(34,0x19c35127);s.Render(28,1);s.vertex.Transform(3,perspective);s.vertex.Viewport({7,9,640,480,0,1});s.ValidateDraw();
    const auto prepared=s.PixelFog();assert(prepared.controls[2]==1 && prepared.parameters[0]==2000 && prepared.parameters[1]==2800);
    const auto viewport=LegacyRasterViewport(s.vertex.Prepare().viewport);assert((viewport==std::array<float,4>{7.5f,9.5f,640,480}));assert(d.NativeReadViewport().X==7 && d.NativeReadViewport().Y==9);
    CDeviceState original(map_device::StateBackend());original.StoreState(DEVSTATE_RENDERSTATES);
    s.Render(28,0);s.Render(34,~0u);s.Render(37,std::bit_cast<unsigned>(9000.f));original.RestoreState(DEVSTATE_RENDERSTATES);
    auto restored=d.state.Read().PixelFog();assert(!std::memcmp(&restored,&prepared,sizeof(prepared)));
    auto lease=d.state.Capture();d.state.Edit().vertex.Transform(3,mat::Identity());d.state.Edit().Render(35,2);assert(lease->Restore()==0);restored=d.state.Read().PixelFog();assert(!std::memcmp(&restored,&prepared,sizeof(prepared)));lease.reset();
    auto invalid=perspective;invalid[11]=2;
    Reject([&]{mat::PreparePixelFog(true,3,0,1,1,0,invalid);});
    Reject([&]{mat::PreparePixelFog(true,0,0,1,1,0,perspective);});
    Reject([&]{mat::PreparePixelFog(true,3,1,1,1,0,perspective);});
    Reject([&]{mat::PreparePixelFog(true,3,2,1,1,0,perspective);});
    Reject([&]{mat::PreparePixelFog(true,1,0,1,-.1f,0,perspective);});
    Reject([&]{mat::PreparePixelFog(true,2,0,1,1.1f,0,perspective);});
    assert(mat::PreparePixelFog(false,0,0,0,0,0,invalid).controls[0]==0);
    for(unsigned mode:{1u,2u}){const auto zero=mat::PreparePixelFog(true,mode,0,1,0,0,perspective);assert(mat::PixelFogFactor(zero,8000)==1);}
    auto& live=d.state.Edit();live.Render(35,0);Reject([&]{live.ValidateDraw();});live.Render(28,0);live.ValidateDraw();
    p::Current().ready=false;Reject([&]{map_device::RasterCaps();});p::Current().device=nullptr;
    std::printf("pixel fog production CPU PASS HAL_goldens=%u max_delta=%u original_scalar_restore=exact lease_restore=exact logical_viewport=preserved native_query=typed unsupported_vertex_projection=refused CPU_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN\n",count,max_delta);
}

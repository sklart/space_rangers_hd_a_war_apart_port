// Production scalar/attachment guards and ORIGINAL Store/Restore against
// actual HAL goldens. CPU tests are not a native GPU or map execution model.
#include "native_stored_state.hpp"
#include "native_map_queries.hpp"
#include "map_device_backend.hpp"
#include "DeviceState.hpp"
#include "stencil_state.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
using namespace srhd_awa::matrixgame;namespace p=platform3g;
template<class F>void Reject(F&& f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
struct Routing final:p::Device,p::NativeMapQueries,p::NativeStoredStateBackend {
    p::NativeStateStore state;mutable unsigned queries{};
    p::NativeStateStore& StoredStates()override{return state;}
    p::VideoViewport NativeReadViewport()const override{return state.Read().vertex.Viewport();}
    p::DeviceResult NativeCooperativeLevel()const override{throw p::Unsupported("stencil CPU test has no GPU queue");}
    std::uint32_t NativeStencilCaps()const override{++queries;return srhd_awa::matrixgame::NativeStencilCaps;}
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("stencil CPU test has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("stencil CPU test has no GPU budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("stencil CPU test has no VI gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
struct Golden {unsigned enabled,two_sided,clockwise,depth_pass,initial,reference,read_mask,write_mask;unsigned cw[4],ccw[4];unsigned value,color_pass;};
static_assert(sizeof(Golden)==72);
int main(int argc,char** argv){assert(argc==2);std::ifstream input(argv[1],std::ios::binary);assert(input);Golden g{};unsigned count=0;
    while(input.read(reinterpret_cast<char*>(&g),sizeof(g))){MeshStencilState s;s.enabled=g.enabled;s.two_sided=g.two_sided;s.reference=g.reference;s.read_mask=g.read_mask;s.write_mask=g.write_mask;
        s.clockwise={g.cw[0],g.cw[1],g.cw[2],g.cw[3]};s.counterclockwise={g.ccw[0],g.ccw[1],g.ccw[2],g.ccw[3]};const auto result=EvaluateStencil(s,g.clockwise,g.depth_pass,g.initial);
        assert(result.value==g.value && result.color_pass==bool(g.color_pass));++count;
    }
    assert(input.eof() && input.gcount()==0 && count==2310);
    Routing d;p::Current().device=&d;p::Current().ready=true;assert(map_device::StencilCaps()==NativeStencilCaps && d.queries==1);
    auto& state=d.state.Edit();for(auto pair:{std::pair{9u,1u},std::pair{52u,1u},std::pair{185u,1u},std::pair{53u,2u},std::pair{54u,6u},std::pair{55u,7u},std::pair{56u,4u},std::pair{57u,0x12345u},std::pair{58u,0x37u},std::pair{59u,0x81u},std::pair{186u,1u},std::pair{187u,4u},std::pair{188u,8u},std::pair{189u,5u}})state.Render(pair.first,pair.second);
    state.ValidateDraw();const auto before=state.Stencil();assert(before.enabled && before.two_sided && state.Render(9)==1 && before.Reference8()==0x45);
    RequireStencilAttachment(before,true,75);Reject([&]{RequireStencilAttachment(before,false,75);});Reject([&]{RequireStencilAttachment(before,true,77);});Reject([&]{RequireStencilAttachment(before,true,80);});
    CDeviceState original(map_device::StateBackend());original.StoreState(DEVSTATE_RENDERSTATES);
    state.Render(9,2);state.Render(185,0);state.Render(57,0);state.Render(53,1);state.Render(188,1);original.RestoreState(DEVSTATE_RENDERSTATES);assert(d.state.Read().Stencil()==before && d.state.Read().shade==1);
    auto lease=d.state.Capture();d.state.Edit().Render(52,0);d.state.Edit().Render(9,2);d.state.Edit().Render(59,0);assert(lease->Restore()==0);assert(d.state.Read().Stencil()==before && d.state.Read().shade==1);lease.reset();assert(d.state.Outstanding()==0);
    auto& live=d.state.Edit();live.Render(185,0);assert(live.Stencil().Face(false)==before.clockwise);live.Render(52,0);RequireStencilAttachment(live.Stencil(),false,0);
    Reject([&]{live.Render(9,3);});assert(live.shade==1);Reject([&]{live.Render(55,9);});Reject([&]{live.Render(189,0);});
    p::Current().ready=false;Reject([&]{map_device::StencilCaps();});p::Current().device=nullptr;
    std::printf("production stencil CPU PASS HAL_goldens=%u exact original_scalar_restore=exact lease_restore=exact flat=real_state D24S8_guard=checked typed_caps=checked CPU_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN\n",count);
}

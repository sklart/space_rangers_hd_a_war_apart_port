// SPDX-License-Identifier: GPL-2.0-or-later
// Negative routing fixture: reaching native draw throws, never claims GPU work.
#ifdef NDEBUG
#error Native routing assertions required
#endif
#include "native_graphics.hpp"
#include "map_device_backend.hpp"
#include <cassert>
#include <cstdio>
namespace p=srhd_awa::matrixgame::platform3g;
namespace map=srhd_awa::matrixgame::map_device;
struct Reached {};
struct Fixture final:p::Device,p::NativeGraphics {
    unsigned transforms{},draws{},immediate{},indexed{},identities{};
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("fixture cannot negotiate GPU caps");}
    p::LegacyDeviceHandles ReadLegacyHandles()override{++identities;throw std::logic_error("native routing requested legacy identity");}
    p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("fixture cannot read gamma");}
    void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("fixture has no GPU memory");}
    int NativeTransform(unsigned t,const p::WorldMatrix& m)override{assert((t==2 || t==3) && m[12]==7);++transforms;throw Reached{};}
    int NativeDraw(unsigned type,unsigned first,unsigned count)override{assert(type==4 && first==9 && count==3);++draws;throw Reached{};}
    int NativeDrawUP(unsigned type,unsigned count,std::span<const std::byte> bytes,unsigned stride)override{assert(type==4 && count==1 && bytes.size()==36 && stride==12);++immediate;throw Reached{};}
    int NativeDrawIndexed(unsigned type,int base,unsigned minimum,unsigned vertices,unsigned start,unsigned count)override{assert(type==4 && base==-5 && minimum==6 && vertices==7 && start==8 && count==9);++indexed;throw Reached{};}
};
template<class F> void Native(F fn){bool reached=false;try{fn();}catch(const Reached&){reached=true;}assert(reached);}
int main(){
    Fixture device;p::Current().ready=true;p::Current().device=&device;
    p::WorldMatrix value{};value[12]=7;
    Native([&]{map::SetTransform(map::Transform::View,value);});Native([&]{map::SetTransform(map::Transform::Projection,value);});
    Native([&]{map::DrawPrimitive(4,9,3);});Native([&]{map::DrawIndexedPrimitive(4,-5,6,7,8,9);});
    std::array<float,9> points{};Native([&]{map::DrawPrimitiveUP(4,1,points.data(),12);});
    for(auto pair:{std::pair<const void*,unsigned>{nullptr,12},{points.data(),0}}){bool refused=false;try{map::DrawPrimitiveUP(4,1,pair.first,pair.second);}catch(const std::invalid_argument&){refused=true;}assert(refused);}
    assert(device.transforms==2 && device.draws==1 && device.immediate==1 && device.indexed==1 && device.identities==0);
    p::Current()=p::State{};
    std::puts("native map view/projection, primitive, UP/indexed routing and invalid source refusal PASS; no D3D identities; GPU NOT RUN");
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Original GenRamp/ApplyGammaRamp plus the production presentation state/LUT.
#include "native_gamma.hpp"
#include "native_diagnostic_pixels.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
using WORD=std::uint16_t;
#include "original-gamma.inc"
template<class F>void Reject(F&& f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
static bool Equal(const p::GammaRamp& a,const p::GammaRamp& b){return !std::memcmp(&a,&b,sizeof(a));}
struct Routing final:p::Device {
    p::NativeGammaStore gamma;
    p::DeviceCaps ReadCaps()override{return {false,8,16,1,8};}
    p::GammaRamp ReadGamma(unsigned index)override{return gamma.Read(index);}
    void ApplyCalibratedGamma(unsigned index,const p::GammaRamp& ramp)override{gamma.Apply(index,ramp);}
    void RestoreGamma(unsigned index,const p::GammaRamp& ramp)noexcept override{gamma.Restore(index,ramp);}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("CPU gamma test has no GPU allocator");}
};
int main(int argc,char** argv){
    assert(argc==2);Routing device;auto& current=p::Current();current.device=&device;current.ready=true;
    const auto identity=p::IdentityGamma();assert(Equal(device.ReadGamma(0),identity));
    auto baseline=device.gamma.Capture();std::vector<p::GammaRamp> ramps{identity};
    p::GammaRamp arbitrary;
    for(unsigned i=0;i<256;++i){arbitrary.red[i]=std::uint16_t((255-i)*257);arbitrary.green[i]=std::uint16_t((i*251u+1037u)&65535);arbitrary.blue[i]=i<128?0:65535;}
    ramps.push_back(arbitrary);ramps.push_back(srhd_awa::matrixgame::diagnostic::SmokeGammaRamp());
    CMatrixConfig config;
    for(float brightness:{.25f,.5f,.75f})for(float contrast:{.25f,.5f})for(float gamma:{.5f,1.f,2.f}){
        config.m_GammaR={brightness,contrast,gamma};config.m_GammaG={.5f,contrast,1.f};config.m_GammaB={brightness,.25f,gamma};
        config.ApplyGammaRamp();const auto actual=device.ReadGamma(0);
        p::GammaRamp expected;GenRamp(expected.red.data(),config.m_GammaR);GenRamp(expected.green.data(),config.m_GammaG);GenRamp(expected.blue.data(),config.m_GammaB);
        assert(Equal(actual,expected));ramps.push_back(actual);
    }
    std::uint64_t samples=0;
    for(const auto& ramp:ramps){
        device.ApplyCalibratedGamma(0,ramp);const auto u=p::PrepareGamma(device.ReadGamma(0));
        const std::array<const std::array<WORD,256>*,3> original{&ramp.red,&ramp.green,&ramp.blue};
        for(unsigned c=0;c<3;++c)for(unsigned i=0;i<65536;++i){
            const double x=double(i)*255.0/65535.0;const auto lo=unsigned(x),hi=std::min(lo+1,255u);
            const double expected=(double((*original[c])[lo])+(double((*original[c])[hi])-(*original[c])[lo])*(x-lo))/65535.0;
            const auto got=p::GammaComponent(u,c,float(i)/65535.f);assert(std::abs(got-expected)<.00004);++samples;
        }
    }
    device.ApplyCalibratedGamma(0,arbitrary);auto snapshot=device.gamma.Capture();
    device.ApplyCalibratedGamma(0,identity);assert(snapshot->Restore()==0 && Equal(device.ReadGamma(0),arbitrary));
    assert(snapshot->Restore()==0);assert(baseline->Restore()==0 && Equal(device.ReadGamma(0),identity));
    Reject([&]{device.ReadGamma(1);});Reject([&]{device.ApplyCalibratedGamma(1,arbitrary);});assert(Equal(device.ReadGamma(0),identity));
    Reject([&]{p::GammaComponent(p::PrepareGamma(identity),3,0);});Reject([&]{p::GammaComponent(p::PrepareGamma(identity),0,NAN);});
    auto orphan=device.gamma.Capture();device.gamma.Close();assert(orphan->Restore()==p::presentation::DeviceLost);
    Reject([&]{device.ReadGamma(0);});Reject([&]{device.gamma.Capture();});
    current.device=nullptr;current.ready=false;
    std::array<std::byte,8*8*4> frame{};const auto center=(4*8+4)*4;
    auto check=[&]{srhd_awa::matrixgame::diagnostic::CheckGammaSmokePixels(8,8,frame);};
    Reject(check);frame[center]=std::byte{191};frame[center+1]=std::byte{128};frame[center+2]=std::byte{64};frame[center+3]=std::byte{255};check();
    frame[center]=std::byte{64};frame[center+2]=std::byte{191};Reject(check);
    static_assert(sizeof(p::GammaRamp)==1536);
    std::ofstream output(argv[1],std::ios::binary);output.write(reinterpret_cast<const char*>(ramps.data()),ramps.size()*sizeof(p::GammaRamp));assert(output.good());
    std::printf("ORIGINAL gamma calls=%zu; production LUT samples=%llu; ownership/restore/invalid-index/closed lifetime; diagnostic final-pixel checker PASS; CPU ONLY NO_NATIVE_INIT\n",ramps.size()-3,static_cast<unsigned long long>(samples));
}

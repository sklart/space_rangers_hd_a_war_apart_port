// Same bootstrap driver; keep native game API names out of the Windows SDK TU.
#include "init_runtime.hpp"
#include "MatrixGameDll.hpp"
#include "MatrixSampleStateManager.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapSamplerCold() {
    assert(g_Sampler.m_AFDegree==0);
    bool rejected=false;try{g_Sampler.SetState(0,SamplerRegister::MinFilter,2);}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected);
}
namespace {
// A backend can lack sampler support even after returning real device caps.
// Forward every queried fact; deny the operation explicitly, without fake HRESULTs.
struct RefusingSampler final:platform3g::Device {
    platform3g::Device& real;
    explicit RefusingSampler(platform3g::Device& device):real(device){}
    platform3g::DeviceCaps ReadCaps()override{return real.ReadCaps();}
    platform3g::GammaRamp ReadGamma(std::uint32_t i)override{return real.ReadGamma(i);}
    void RestoreGamma(std::uint32_t i,const platform3g::GammaRamp& r)noexcept override{real.RestoreGamma(i,r);}
    std::uint64_t AvailableTextureBytes()override{return real.AvailableTextureBytes();}
    platform3g::DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t)override {
        throw platform3g::Unsupported("sampler operation denied by backend");
    }
};
}
void VerifyBootstrapSampling(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,unsigned cycle,bool negotiated) {
    SRobotsSettings settings{};
    if(!negotiated) {
        bool rejected=false;try{runtime.ApplySamplerSettings(settings);}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected);
        rejected=false;try{g_Sampler.SetState(0,SamplerRegister::MinFilter,2);}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected);std::puts("original sampler blocked before device negotiation");return;
    }
    settings.m_AFDegree=cycle?0:64;
    runtime.ApplySamplerSettings(settings);
    const auto expected=std::min(platform3g::Current().caps.max_anisotropy,static_cast<std::uint32_t>(settings.m_AFDegree));
    assert(static_cast<std::uint32_t>(g_Sampler.m_AFDegree)==expected);
    for(auto filter:{SamplerRegister::MagFilter,SamplerRegister::MinFilter,SamplerRegister::MipFilter}) {
        const auto result=g_Sampler.SetState(0,filter,static_cast<std::uint32_t>(TextureFilter::Linear));
        assert(result==platform3g::DeviceSuccess);
        assert(device.ReadSamplerState(0,filter)==static_cast<std::uint32_t>(expected?TextureFilter::Anisotropic:TextureFilter::Linear));
        if(expected)assert(device.ReadSamplerState(0,SamplerRegister::MaxAnisotropy)==expected);
    }
    // Unrelated sampler states must pass through the original manager unchanged.
    assert(g_Sampler.SetState(0,SamplerRegister::AddressU,static_cast<std::uint32_t>(TextureAddress::Clamp))==platform3g::DeviceSuccess);
    assert(device.ReadSamplerState(0,SamplerRegister::AddressU)==static_cast<std::uint32_t>(TextureAddress::Clamp));
    // NULLREF accepts even invalid sampler enums, so it cannot prove the
    // negative HRESULT branch. Exercise explicit backend rejection instead.
    runtime.DetachDevice();RefusingSampler refusing(device);
    struct Guard {NativeInitRuntime& runtime;bool armed{true};~Guard(){if(armed)runtime.DetachDevice();}} guard{runtime};
    runtime.AttachDevice(window,refusing);runtime.ApplySamplerSettings(settings);
    bool rejected=false;try{g_Sampler.SetState(0,SamplerRegister::MinFilter,2);}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected);
    runtime.DetachDevice();guard.armed=false;runtime.AttachDevice(window,device);runtime.ApplySamplerSettings(settings);
    assert(g_Sampler.SetState(0,SamplerRegister::MinFilter,2)==platform3g::DeviceSuccess);
    std::printf("original full sampler TU EXECUTED AF=%u; real SDK readback + explicit rejection/reentry PASS\n",expected);
}

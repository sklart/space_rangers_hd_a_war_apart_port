// Same bootstrap driver; keep native game API names out of the Windows SDK TU.
#include "init_runtime.hpp"
#include "MatrixGameDll.hpp"
#include "MatrixSampleStateManager.hpp"
#include "DeviceState.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <bit>
#include <utility>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapSamplerCold() {
    assert(g_Sampler.m_AFDegree==0);
    bool rejected=false;try{g_Sampler.SetState(0,SamplerRegister::MinFilter,2);}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected);
    struct DeniedStateBackend final:platform3g::StoredStateBackend {
        platform3g::DeviceResult GetRenderState(platform3g::StoredRenderRegister,std::uint32_t*)override{throw platform3g::Unsupported("no actual render query backend");}
        platform3g::DeviceResult SetRenderState(platform3g::StoredRenderRegister,std::uint32_t)override{throw platform3g::Unsupported("no actual render restore backend");}
        platform3g::DeviceResult GetSamplerState(std::uint32_t,platform3g::StoredSamplerRegister,std::uint32_t*)override{throw platform3g::Unsupported("no actual sampler query backend");}
        platform3g::DeviceResult SetSamplerState(std::uint32_t,platform3g::StoredSamplerRegister,std::uint32_t)override{throw platform3g::Unsupported("no actual sampler restore backend");}
    } denied;
    CDeviceState states(&denied);
    bool uncaptured=false;try{states.RestoreState(DEVSTATE_RENDERSTATES);}catch(const std::logic_error&){uncaptured=true;}
    assert(uncaptured);
    bool capture_denied=false;try{states.StoreState(DEVSTATE_RENDERSTATES);}catch(const platform3g::Unsupported&){capture_denied=true;}
    assert(capture_denied);
    uncaptured=false;try{states.RestoreState(DEVSTATE_RENDERSTATES);}catch(const std::logic_error&){uncaptured=true;}
    assert(uncaptured);
    std::puts("full original DeviceState TU: no-backend capture denied; uncaptured/failed groups cannot restore");
}
namespace {
// A backend can lack sampler support even after returning real device caps.
// Forward every queried fact; deny the operation explicitly, without fake HRESULTs.
struct ForwardDevice:platform3g::Device {
    platform3g::Device& real;
    explicit ForwardDevice(platform3g::Device& device):real(device){}
    platform3g::DeviceCaps ReadCaps()override{return real.ReadCaps();}
    platform3g::GammaRamp ReadGamma(std::uint32_t i)override{return real.ReadGamma(i);}
    void RestoreGamma(std::uint32_t i,const platform3g::GammaRamp& r)noexcept override{real.RestoreGamma(i,r);}
    std::uint64_t AvailableTextureBytes()override{return real.AvailableTextureBytes();}
    std::unique_ptr<platform3g::DeviceStateLease> CaptureState()override{return real.CaptureState();}
    platform3g::DeviceResult SetSamplerState(std::uint32_t s,SamplerRegister t,std::uint32_t v)override{return real.SetSamplerState(s,t,v);}
    platform3g::DeviceResult SetWorldTransform(const platform3g::WorldMatrix& m)override{return real.SetWorldTransform(m);}
    platform3g::DeviceResult ClearTexture(std::uint32_t s)override{return real.ClearTexture(s);}
    platform3g::DeviceResult SetTextureStageState(std::uint32_t s,platform3g::TextureStageRegister t,std::uint32_t v)override{return real.SetTextureStageState(s,t,v);}
};
struct RefusingSampler final:ForwardDevice {
    using ForwardDevice::ForwardDevice;
    platform3g::DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t)override {
        throw platform3g::Unsupported("sampler operation denied by backend");
    }
};
struct RefusingDefaults final:ForwardDevice {
    unsigned worlds{},stages{};using ForwardDevice::ForwardDevice;
    platform3g::DeviceResult SetWorldTransform(const platform3g::WorldMatrix& m)override{++worlds;return real.SetWorldTransform(m);}
    platform3g::DeviceResult SetTextureStageState(std::uint32_t s,platform3g::TextureStageRegister t,std::uint32_t v)override{++stages;return real.SetTextureStageState(s,t,v);}
    platform3g::DeviceResult SetRenderState(RasterRegister,std::uint32_t)override{throw platform3g::Unsupported("render state denied after actual world/sampler writes");}
};
struct RefusingCapture final:ForwardDevice {
    using ForwardDevice::ForwardDevice;
    std::unique_ptr<platform3g::DeviceStateLease> CaptureState()override{throw platform3g::Unsupported("state capture denied after actual caps negotiation");}
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
void VerifyBootstrapDefaults(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,unsigned cycle,bool negotiated) {
    if(!negotiated) {
        bool rejected=false;try{runtime.ApplyDefaultDeviceState();}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected && !runtime.DefaultDeviceStateReady());return;
    }
    SRobotsSettings settings{};settings.m_AFDegree=cycle?0:64;
    // Model foreign caller state BEFORE acquisition. Identity/default blend
    // would let a missing Restore accidentally pass after S3D_Default.
    runtime.DetachDevice();
    auto foreign_world=device.ReadWorldTransform();foreign_world[12]=137.5f;foreign_world[13]=-11.f;
    platform3g::CheckDeviceResult(device.SetWorldTransform(foreign_world),"foreign world before attach");
    platform3g::CheckDeviceResult(device.SetRenderState(RasterRegister::BlendEnable,1),"foreign blend before attach");
    for(unsigned stage=0;stage<8;++stage) {
        platform3g::CheckDeviceResult(device.SetSamplerState(stage,SamplerRegister::AddressU,static_cast<unsigned>(TextureAddress::Clamp)),"foreign sampler");
        platform3g::CheckDeviceResult(device.SetTextureStageState(stage,platform3g::TextureStageRegister::TexcoordIndex,1),"foreign texcoord");
    }
    assert(device.TextureBound(0));
    runtime.AttachDevice(window,device);runtime.ApplySamplerSettings(settings);
    const auto original_world=device.ReadWorldTransform();
    const auto original_blend=device.ReadRenderState(RasterRegister::BlendEnable);
    auto* stored_backend=dynamic_cast<platform3g::StoredStateBackend*>(&device);
    assert(stored_backend); // Windows SDK provider implements actual complete tables.
    const auto read_stored=[&] {
        std::array<std::uint32_t,103+13*8> values{};std::size_t i=0;
        for(unsigned key=0;key<103;++key)
            platform3g::CheckDeviceResult(stored_backend->GetRenderState(static_cast<platform3g::StoredRenderRegister>(key),&values[i++]),"verify actual stored render");
        for(unsigned stage=0;stage<8;++stage)for(unsigned key=0;key<13;++key)
            platform3g::CheckDeviceResult(stored_backend->GetSamplerState(stage,static_cast<platform3g::StoredSamplerRegister>(key),&values[i++]),"verify actual stored sampler");
        return values;
    };
    const auto original_stored=read_stored();
    CDeviceState selective(stored_backend);
    bool all_denied=false;try{selective.StoreState();}catch(const platform3g::Unsupported&){all_denied=true;}
    assert(all_denied); // Actual 207 queries precede the unsupported light group.
    bool invalidated=false;try{selective.RestoreState(DEVSTATE_RENDERSTATES);}catch(const std::logic_error&){invalidated=true;}
    assert(invalidated && read_stored()==original_stored);
    selective.StoreState(DEVSTATE_RENDERSTATES);
    platform3g::CheckDeviceResult(stored_backend->SetRenderState(platform3g::StoredRenderRegister::FILLMODE,2),"selective render mutation");
    platform3g::CheckDeviceResult(stored_backend->SetSamplerState(7,platform3g::StoredSamplerRegister::BORDERCOLOR,0x12345678),"selective sampler mutation");
    selective.RestoreState(DEVSTATE_RENDERSTATES);
    auto selective_values=read_stored();
    assert(std::equal(selective_values.begin(),selective_values.begin()+103,original_stored.begin()));
    assert(selective_values[103+7*13+3]==0x12345678);
    platform3g::CheckDeviceResult(stored_backend->SetSamplerState(7,platform3g::StoredSamplerRegister::BORDERCOLOR,original_stored[103+7*13+3]),"reset selective sampler");
    selective.StoreState(DEVSTATE_RENDERSTATES|DEVSTATE_SAMPLERSTATES);
    selective.RestoreState(DEVSTATE_RENDERSTATES|DEVSTATE_SAMPLERSTATES);
    assert(read_stored()==original_stored && device.OutstandingStateLeases()==1);
    std::puts("CDeviceState selective groups + real-query partial StoreState(ALL) rejection + recapture PASS; lights/material Unsupported");
    const auto restored=[&] {
        assert(read_stored()==original_stored);
        assert(device.ReadWorldTransform()==original_world && device.ReadRenderState(RasterRegister::BlendEnable)==original_blend);
        assert(device.TextureBound(0));
        for(unsigned stage=0;stage<8;++stage) {
            assert(device.ReadSamplerState(stage,SamplerRegister::AddressU)==static_cast<unsigned>(TextureAddress::Clamp));
            assert(device.ReadTextureStageState(stage,platform3g::TextureStageRegister::TexcoordIndex)==1);
        }
    };
    auto altered=original_world;altered[12]=137.5f;
    platform3g::CheckDeviceResult(device.SetWorldTransform(altered),"seed world");
    platform3g::CheckDeviceResult(device.SetRenderState(RasterRegister::BlendEnable,original_blend?0:1),"seed blend");
    platform3g::CheckDeviceResult(stored_backend->SetRenderState(platform3g::StoredRenderRegister::FILLMODE,2),"mutate extra captured render");
    platform3g::CheckDeviceResult(stored_backend->SetSamplerState(7,platform3g::StoredSamplerRegister::BORDERCOLOR,0x12345678),"mutate extra captured sampler");
    assert(read_stored()!=original_stored);
    const auto check=[&] {
        assert(runtime.DefaultDeviceStateReady());
        const platform3g::WorldMatrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        assert(device.ReadWorldTransform()==identity);
        const auto filter=g_Sampler.m_AFDegree?TextureFilter::Anisotropic:TextureFilter::Linear;
        for(unsigned stage=0;stage<8;++stage) {
            for(auto key:{SamplerRegister::MagFilter,SamplerRegister::MinFilter,SamplerRegister::MipFilter})
                assert(device.ReadSamplerState(stage,key)==static_cast<std::uint32_t>(filter));
            assert(device.ReadSamplerState(stage,SamplerRegister::AddressU)==static_cast<unsigned>(TextureAddress::Repeat));
            assert(device.ReadSamplerState(stage,SamplerRegister::AddressV)==static_cast<unsigned>(TextureAddress::Repeat));
            assert(device.ReadSamplerState(stage,SamplerRegister::LodBias)==std::bit_cast<std::uint32_t>(-1.f));
            if(g_Sampler.m_AFDegree)assert(device.ReadSamplerState(stage,SamplerRegister::MaxAnisotropy)==static_cast<std::uint32_t>(g_Sampler.m_AFDegree));
            assert(device.ReadTextureStageState(stage,platform3g::TextureStageRegister::TexcoordIndex)==0);
            assert(!device.TextureBound(stage));
        }
        const std::pair<RasterRegister,std::uint32_t> expected[]{
            {RasterRegister::ShadeMode,2},{RasterRegister::BlendEnable,0},{RasterRegister::SourceBlend,5},{RasterRegister::DestinationBlend,6},
            {RasterRegister::AlphaTest,0},{RasterRegister::AlphaReference,8},{RasterRegister::AlphaCompare,7},{RasterRegister::CullMode,3},
            {RasterRegister::Lighting,0},{RasterRegister::DepthTest,1},{RasterRegister::DepthWrite,1},{RasterRegister::DepthCompare,4}};
        for(const auto& [reg,value]:expected)assert(device.ReadRenderState(reg)==value);
    };
    runtime.ApplyDefaultDeviceState();check();assert(device.OutstandingStateLeases()==1);
    runtime.DetachDevice();
    assert(runtime.LastStateRestoreResult()==platform3g::DeviceSuccess && device.OutstandingStateLeases()==0);
    std::printf("restore readback world.x=%g expected=%g blend=%u expected=%u\n",device.ReadWorldTransform()[12],original_world[12],device.ReadRenderState(RasterRegister::BlendEnable),original_blend);
    restored();
    std::puts("original full CDeviceState TU EXECUTED: 103 render + 13x8 sampler; all 207 values restored by actual SDK readback");
    RefusingDefaults refusing(device);
    struct Guard {NativeInitRuntime& runtime;bool armed{true};~Guard(){if(armed)runtime.DetachDevice();}} guard{runtime};
    runtime.AttachDevice(window,refusing);runtime.ApplySamplerSettings(settings);
    platform3g::CheckDeviceResult(device.SetWorldTransform(altered),"seed partial-init world");
    bool rejected=false;try{runtime.ApplyDefaultDeviceState();}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected && refusing.worlds==1 && refusing.stages==8);
    assert(!runtime.GpuReady() && !runtime.DefaultDeviceStateReady() && runtime.CpuReady());
    assert(runtime.LastStateRestoreResult()==platform3g::DeviceSuccess && device.OutstandingStateLeases()==0);
    restored();
    RefusingCapture no_capture(device);bool capture_rejected=false;
    try{runtime.AttachDevice(window,no_capture);}catch(const platform3g::Unsupported&){capture_rejected=true;}
    assert(capture_rejected && runtime.CpuReady() && InspectCpu3GState().bound && !runtime.GpuReady());
    assert(!platform3g::Current().attachment && device.OutstandingStateLeases()==0);
    guard.armed=false;runtime.AttachDevice(window,device);runtime.ApplySamplerSettings(settings);
    runtime.ApplyDefaultDeviceState();check();
    std::puts("original S3D_Default EXECUTED: world + 8 sampler/texture stages + 12 render states; actual stateblock restore/fault/reentry PASS");
}

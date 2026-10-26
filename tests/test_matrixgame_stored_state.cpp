// Original CDeviceState + production native scalar adapter, no GPU fixture.
#include "native_stored_state.hpp"
#include "native_state_layout.hpp"
#include "map_device_backend.hpp"
#include "DeviceState.hpp"
#include <cassert>
#include <bit>
#include <cstdio>
#include <cstring>
using namespace srhd_awa::matrixgame;
namespace p=platform3g;
template<class F>void Reject(F&& f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
struct Routing final:p::Device,p::NativeStoredStateBackend {
    using p::NativeStoredStateBackend::SetSamplerState;
    using p::NativeStoredStateBackend::SetRenderState;
    p::NativeStateStore state;
    unsigned reads{},fail_read{},fail_light{};
    p::NativeStateStore& StoredStates()override{return state;}
    p::DeviceResult GetRenderState(p::StoredRenderRegister r,std::uint32_t* out)override{
        if(++reads==fail_read)return static_cast<std::int32_t>(0x88760868u);
        return NativeStoredStateBackend::GetRenderState(r,out);
    }
    p::DeviceResult GetLight(std::uint32_t i,p::StoredLight* out)override{
        if(fail_light==i+1)return static_cast<std::int32_t>(0x88760868u);
        return NativeStoredStateBackend::GetLight(i,out);
    }
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("scalar test has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("scalar test has no GPU budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("scalar test has no display gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
struct Scalar {
    std::array<std::uint32_t,103> render;
    std::array<std::array<std::uint32_t,13>,8> sampler;
    std::array<p::StoredLight,8> lights;
    std::array<std::int32_t,8> enabled;
    p::StoredMaterial material;
};
Scalar Read(Routing& d){Scalar s{};
    for(unsigned i=0;i<103;++i)assert(d.GetRenderState(p::StoredRenderRegister(i),&s.render[i])==0);
    for(unsigned slot=0;slot<8;++slot){for(unsigned i=0;i<13;++i)assert(d.GetSamplerState(slot,p::StoredSamplerRegister(i),&s.sampler[slot][i])==0);
        assert(d.GetLight(slot,&s.lights[slot])==0 && d.GetLightEnable(slot,&s.enabled[slot])==0);}
    assert(d.GetMaterial(&s.material)==0);return s;
}
void Same(Routing& d,const Scalar& s){const auto a=Read(d);assert(a.render==s.render && a.sampler==s.sampler && a.enabled==s.enabled);
    static_assert(sizeof(p::StoredMaterial)==68 && sizeof(p::StoredLight)==104);
    assert(!std::memcmp(&a.material,&s.material,sizeof(s.material)));
    for(unsigned i=0;i<8;++i)assert(!std::memcmp(&a.lights[i],&s.lights[i],sizeof(s.lights[i])));
}
void Configure(Routing& d){auto& s=d.state.Edit();
    s.Render(22,1);s.Render(24,237);s.Render(60,0x8173ca12);s.Render(139,0x91a7b3ff);
    s.Render(34,0xff765432); // Fog color remains meaningful scalar state while fog is off.
    s.Render(36,std::bit_cast<std::uint32_t>(3.5f));s.Render(37,std::bit_cast<std::uint32_t>(220.f));
    s.Render(57,123);s.Render(58,0x37);s.Render(59,0x81); // Stencil parameters, disabled.
    s.Render(190,7);s.Render(193,0x11335779); // Target2 mask and blend constant.
    s.vertex.Render(137,1);s.vertex.Render(143,1); // Query must read actual shader state, not a stale mirror.
    p::VideoMaterial m{};m.Diffuse={.2f,.3f,.4f,.7f};m.Ambient={.1f,.2f,.3f,.4f};m.Specular={.8f,.6f,.4f,.2f};m.Emissive={.01f,.02f,.03f,.04f};m.Power=37.5f;s.vertex.Material(m);
    for(unsigned i=0;i<8;++i){s.samplers[i].Apply(SamplerRegister::MaxAnisotropy,i+1);s.Sampler(i,4,0xaabbcc00+i);s.Sampler(i,8,std::bit_cast<std::uint32_t>(-.125f*float(i)));
        if(i==7)continue;
        p::VideoLight l{};l.Type=3;l.Direction={1.f+float(i),-2,3};l.Position={10.f+float(i),-12,31};l.Diffuse={.7f,.6f,.5f,.4f};l.Specular={.11f,.22f,.33f,.44f};l.Ambient={.1f,.2f,.3f,1};
        l.Range=100+i;l.Falloff=1.5f;l.Attenuation0=1;l.Attenuation1=.01f;l.Attenuation2=.02f;l.Theta=.3f;l.Phi=.7f;
        s.vertex.Light(i,l);s.vertex.EnableLight(i,i%2==0);
    }
}
int main(){Routing d;p::Current().device=&d;p::Current().ready=true;
#ifndef _WIN32
    assert(map_device::StateBackend()==static_cast<p::StoredStateBackend*>(&d));
    CDeviceState saved(map_device::StateBackend());
#else
    CDeviceState saved(static_cast<p::StoredStateBackend*>(&d));
#endif
    Reject([&]{saved.RestoreState();});
    for(unsigned byte=0;byte<256;++byte)for(unsigned channel=0;channel<4;++channel){
        const auto argb=byte<<(channel*8);auto& s=d.state.Edit();s.combiner.Factor(argb);s.vertex.Render(139,argb);
        assert(s.Render(60)==argb && s.Render(139)==argb);
    }
    Configure(d);const auto before=Read(d);const auto shader_before=d.state.Read().vertex.Prepare();saved.StoreState();assert(d.state.Outstanding()==0);
    // Scalar capture must not pin the default texture or bound staging buffers.
    auto session=std::make_shared<native_texture::Session>();auto texture=std::make_shared<native_texture::Texture>();texture->session=session;texture->pool=0;
    session->textures.push_back(texture);std::weak_ptr<native_texture::Texture> weak=texture;d.state.Edit().textures[0]=texture;texture.reset();
    buffers::Context context;buffers::Scope scope(context);auto* vb=context.CreateVertex(48,2);context.BindVertex(vb,0,12);vb->Release();assert(context.AllocatedBytes()==48);
    saved.StoreState();assert(session->DefaultResourcesLive());
    auto lease=d.state.Capture();d.state.Edit().Render(24,41);assert(lease->Restore()==0);lease.reset();Same(d,before);
    for(unsigned cycle=0;cycle<3;++cycle){d.state.ResetPrepared(std::make_shared<p::NativeRenderState>());
        assert(!session->DefaultResourcesLive() && weak.expired() && context.AllocatedBytes()==0);
        saved.RestoreState();Same(d,before);assert(d.state.Outstanding()==0);
        const auto shader=d.state.Read().vertex.Prepare();assert(!std::memcmp(&shader,&shader_before,sizeof(shader)));
    }
    // Original Type=0 rule: restoring an absent definition disables a light,
    // but deliberately does not erase a definition installed after capture.
    p::StoredLight seventh=before.lights[0];seventh.Direction={3,-4,5};assert(d.SetLight(7,&seventh)==0 && d.LightEnable(7,1)==0);
    saved.RestoreState(DEVSTATE_LIGHTS);p::StoredLight retained{};std::int32_t enabled=1;
    assert(d.GetLight(7,&retained)==0 && d.GetLightEnable(7,&enabled)==0 && !enabled);
    assert(!std::memcmp(&retained,&seventh,sizeof(retained)));
    // Failed recapture invalidates only the requested original group.
    d.fail_read=d.reads+8;Reject([&]{saved.StoreState(DEVSTATE_RENDERSTATES);});d.fail_read=0;
    Reject([&]{saved.RestoreState(DEVSTATE_RENDERSTATES);});saved.RestoreState(DEVSTATE_MATERIAL);
    saved.StoreState(DEVSTATE_RENDERSTATES);saved.RestoreState(DEVSTATE_RENDERSTATES);
    d.fail_light=4;Reject([&]{saved.StoreState(DEVSTATE_LIGHTS);});d.fail_light=0;
    Reject([&]{saved.RestoreState(DEVSTATE_LIGHTS);});saved.StoreState(DEVSTATE_LIGHTS);saved.RestoreState(DEVSTATE_LIGHTS);
    Reject([&]{d.CaptureLightSlot(0,nullptr,&enabled);});Reject([&]{d.CaptureLightSlot(0,&retained,nullptr);});
    Reject([&]{d.GetRenderState(p::StoredRenderRegister::Count,&retained.Type);});
    Reject([&]{d.GetRenderState(p::StoredRenderRegister::ZENABLE,nullptr);});
    Reject([&]{d.GetSamplerState(8,p::StoredSamplerRegister::ADDRESSU,&retained.Type);});
    Reject([&]{d.SetSamplerState(0,p::StoredSamplerRegister::SRGBTEXTURE,2);});
    Reject([&]{d.LightEnable(8,1);});Reject([&]{d.SetMaterial(nullptr);});
    // Inactive values can roundtrip; active unsupported work never reaches GPU.
    auto& s=d.state.Edit();s.ValidateDraw();s.Render(28,1);Reject([&]{s.ValidateDraw();});s.Render(28,0);
    s.Render(174,1);Reject([&]{s.ValidateClear();});s.Render(174,0);
    s.Render(52,1);s.ValidateDraw();assert(s.Stencil().enabled);s.Render(52,0);
    s.Render(162,0);Reject([&]{s.ValidateDraw();});s.Render(162,~0u);
    s.combiner.Set(0,1,4);s.combiner.Set(0,2,2);s.combiner.Set(0,3,0);s.combiner.Bind(0,true);
    s.Sampler(0,9,2);Reject([&]{s.ValidateDraw();});s.Sampler(0,9,0);s.ValidateDraw();
    s.Sampler(0,11,1);Reject([&]{s.ValidateDraw();});s.Sampler(0,11,0);s.ValidateDraw();
    Reject([&]{s.ValidateDraw(true);});Reject([&]{s.ValidateDraw(false,true);});
    auto material=s.vertex.Material();material.Power=std::bit_cast<float>(0x7fc00000u);Reject([&]{s.vertex.Material(material);});assert(s.vertex.Material().Power==37.5f);
    p::Current().device=nullptr;p::Current().ready=false;
    std::puts("original CDeviceState/native scalar PASS render=103 samplers=13x8 lights=8 material=full argb_cases=1024 shader_uniforms=1328_exact restore/reset=3 no_texture_buffer_retention absent_light=original failed_recapture=guarded active_unsupported=refused CPU_SCALAR_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN");
}

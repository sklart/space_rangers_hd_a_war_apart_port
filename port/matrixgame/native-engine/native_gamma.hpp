// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "native_presentation.hpp"
#include <algorithm>
#include <cmath>
namespace srhd_awa::matrixgame::platform3g {
inline GammaRamp IdentityGamma()noexcept {
    GammaRamp ramp;
    for(unsigned i=0;i<256;++i)ramp.red[i]=ramp.green[i]=ramp.blue[i]=std::uint16_t(i*257u);
    return ramp;
}
// LUT values are kept at the original WORD precision. std140 vec4 stride is
// explicit; alpha is not a gamma-controlled component.
struct alignas(16) GammaUniforms {std::array<std::array<float,4>,256> entries;};
static_assert(sizeof(GammaUniforms)==4096);
inline GammaUniforms PrepareGamma(const GammaRamp& ramp)noexcept {
    GammaUniforms out{};
    for(unsigned i=0;i<256;++i)out.entries[i]={float(ramp.red[i])/65535.f,float(ramp.green[i])/65535.f,float(ramp.blue[i])/65535.f,0};
    return out;
}
// Reference for unorm inputs with fewer than eight bits per component too.
inline float GammaComponent(const GammaUniforms& u,unsigned channel,float value){
    if(channel>=3 || !std::isfinite(value))throw std::invalid_argument("gamma channel/input");
    const float x=std::clamp(value,0.f,1.f)*255.f;const auto lo=unsigned(x),hi=std::min(lo+1,255u);
    return u.entries[lo][channel]+(u.entries[hi][channel]-u.entries[lo][channel])*(x-float(lo));
}
class NativeGammaStore {
    struct Control {GammaRamp ramp=IdentityGamma();bool alive=true;};
    std::shared_ptr<Control> control_=std::make_shared<Control>();
    class Lease final:public DeviceStateLease {
        std::shared_ptr<Control> owner_;GammaRamp saved_;
    public:
        explicit Lease(std::shared_ptr<Control> owner):owner_(std::move(owner)),saved_(owner_->ramp){}
        DeviceResult Restore()noexcept override {
            if(!owner_->alive)return presentation::DeviceLost;
            owner_->ramp=saved_;return DeviceSuccess;
        }
    };
    void Require(unsigned index)const {
        if(index)throw std::invalid_argument("native gamma swapchain index");
        if(!control_->alive)throw Unsupported("native gamma device lifetime ended");
    }
public:
    NativeGammaStore()=default;
    NativeGammaStore(const NativeGammaStore&)=delete;
    NativeGammaStore& operator=(const NativeGammaStore&)=delete;
    ~NativeGammaStore(){Close();}
    GammaRamp Read(unsigned index)const {Require(index);return control_->ramp;}
    void Apply(unsigned index,const GammaRamp& ramp){Require(index);control_->ramp=ramp;}
    void Restore(unsigned index,const GammaRamp& ramp)noexcept {
        if(index || !control_->alive)std::terminate();control_->ramp=ramp;
    }
    std::unique_ptr<DeviceStateLease> Capture(){Require(0);return std::make_unique<Lease>(control_);}
    void Close()noexcept {control_->alive=false;}
};
}

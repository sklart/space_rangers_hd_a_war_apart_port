// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "device_state_registers.hpp"
namespace srhd_awa::matrixgame::platform3g {
// Value payloads of the original algorithm. No SDK interface is impersonated.
struct StoredLight {
    std::uint32_t Type{};
    std::array<float,4> Diffuse{},Specular{},Ambient{};
    std::array<float,3> Position{},Direction{};
    float Range{},Falloff{},Attenuation0{},Attenuation1{},Attenuation2{},Theta{},Phi{};
};
struct StoredMaterial {
    std::array<float,4> Diffuse{},Ambient{},Specular{},Emissive{};
    float Power{};
};
class StoredStateBackend {
public:
    virtual ~StoredStateBackend()=default;
    virtual DeviceResult GetRenderState(StoredRenderRegister,std::uint32_t*)=0;
    virtual DeviceResult SetRenderState(StoredRenderRegister,std::uint32_t)=0;
    virtual DeviceResult GetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t*)=0;
    virtual DeviceResult SetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t)=0;
    virtual DeviceResult GetLight(std::uint32_t,StoredLight*){throw Unsupported("stored light query unsupported");}
    virtual DeviceResult GetLightEnable(std::uint32_t,std::int32_t*){throw Unsupported("stored light enable query unsupported");}
    virtual DeviceResult SetLight(std::uint32_t,const StoredLight*){throw Unsupported("stored light restore unsupported");}
    virtual DeviceResult LightEnable(std::uint32_t,std::int32_t){throw Unsupported("stored light enable restore unsupported");}
    virtual DeviceResult GetMaterial(StoredMaterial*){throw Unsupported("stored material query unsupported");}
    virtual DeviceResult SetMaterial(const StoredMaterial*){throw Unsupported("stored material restore unsupported");}
    // Capture a logical light slot, including an explicitly undefined slot.
    // Raw SDK query methods retain their real HRESULT; this is not GetLight.
    virtual void CaptureLightSlot(std::uint32_t index,StoredLight* light,std::int32_t* enabled){
        if(!light || !enabled)throw std::invalid_argument("null light slot capture output");
        CheckDeviceResult(GetLight(index,light),"capture light slot");
        CheckDeviceResult(GetLightEnable(index,enabled),"capture light slot enable");
    }
};
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "device_state_backend.hpp"
#include "native_device_state.hpp"
namespace srhd_awa::matrixgame::platform3g {
// Adapter for the ORIGINAL CDeviceState scalar algorithm. No texture, surface,
// buffer, frame or lease owners are captured here; map reset can release them.
class NativeStoredStateBackend:public StoredStateBackend {
public:
    DeviceResult GetRenderState(StoredRenderRegister,std::uint32_t*)override;
    DeviceResult SetRenderState(StoredRenderRegister,std::uint32_t)override;
    DeviceResult GetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t*)override;
    DeviceResult SetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t)override;
    DeviceResult GetLight(std::uint32_t,StoredLight*)override;
    DeviceResult GetLightEnable(std::uint32_t,std::int32_t*)override;
    DeviceResult SetLight(std::uint32_t,const StoredLight*)override;
    DeviceResult LightEnable(std::uint32_t,std::int32_t)override;
    DeviceResult GetMaterial(StoredMaterial*)override;
    DeviceResult SetMaterial(const StoredMaterial*)override;
protected:
    virtual NativeStateStore& StoredStates()=0;
};
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Independent real D3D9 SDK readback of the typed slot/material adapter.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include "DeviceState.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace srhd_awa::matrixgame;
void VerifyOriginalStoredSlots(platform3g::Device& device){
    namespace p=platform3g;
    auto& adapter=dynamic_cast<p::D3D9Device&>(device);
    const auto handles=adapter.ReadLegacyHandles();auto& sdk=*reinterpret_cast<IDirect3DDevice9*>(handles.device);
    CDeviceState original(&adapter);original.StoreState(DEVSTATE_LIGHTS|DEVSTATE_MATERIAL);
    static_assert(sizeof(D3DLIGHT9)==sizeof(p::StoredLight));
    static_assert(sizeof(D3DMATERIAL9)==sizeof(p::StoredMaterial));
    D3DLIGHT9 raw{};p::StoredLight value{};value.Type=0xfefefefeu;
    const auto expected=sdk.GetLight(7,&raw);const auto actual=adapter.GetLight(7,&value);assert(actual==expected);
    if(FAILED(expected)){
        assert(value.Type==0xfefefefeu);
        std::int32_t enabled=99;adapter.CaptureLightSlot(7,&value,&enabled);
        assert(value.Type==0 && enabled==0);
    }
    D3DLIGHT9 light{};light.Type=D3DLIGHT_DIRECTIONAL;
    light.Diffuse={.7f,.6f,.5f,.4f};light.Specular={.11f,.22f,.33f,.44f};light.Ambient={.1f,.2f,.3f,1};
    light.Position={11,-12,31};light.Direction={1,-2,3};light.Range=103;light.Falloff=1.5f;
    light.Attenuation0=1;light.Attenuation1=.01f;light.Attenuation2=.02f;light.Theta=.3f;light.Phi=.7f;
    p::CheckDeviceResult(sdk.SetLight(3,&light),"SDK slot oracle SetLight");
    p::CheckDeviceResult(adapter.GetLight(3,&value),"typed slot GetLight");
    p::CheckDeviceResult(sdk.GetLight(3,&raw),"SDK slot oracle GetLight");assert(!std::memcmp(&value,&raw,sizeof(raw)));
    p::CheckDeviceResult(sdk.LightEnable(3,TRUE),"SDK slot oracle enable");
    BOOL raw_enabled{};std::int32_t enabled{};
    p::CheckDeviceResult(sdk.GetLightEnable(3,&raw_enabled),"SDK slot oracle GetEnable");
    p::CheckDeviceResult(adapter.GetLightEnable(3,&enabled),"typed slot GetEnable");assert(enabled==raw_enabled);
    value.Position={17,23,41};value.Specular={.51f,.62f,.73f,.84f};value.Direction={-3,2,1};value.Range=207;
    p::CheckDeviceResult(adapter.SetLight(3,&value),"typed slot SetLight");
    p::CheckDeviceResult(adapter.LightEnable(3,0),"typed slot disable");
    p::CheckDeviceResult(sdk.GetLight(3,&raw),"SDK slot readback");assert(!std::memcmp(&value,&raw,sizeof(raw)));
    p::CheckDeviceResult(sdk.GetLightEnable(3,&raw_enabled),"SDK slot disable readback");assert(!raw_enabled);
    p::StoredMaterial material{{.1f,.2f,.3f,.4f},{.2f,.3f,.4f,.5f},{.3f,.4f,.5f,.6f},{.4f,.5f,.6f,.7f},37.5f};
    p::CheckDeviceResult(adapter.SetMaterial(&material),"typed material Set");D3DMATERIAL9 raw_material{};
    p::CheckDeviceResult(sdk.GetMaterial(&raw_material),"SDK material readback");assert(!std::memcmp(&material,&raw_material,sizeof(material)));
    p::StoredMaterial captured{};p::CheckDeviceResult(adapter.GetMaterial(&captured),"typed material Get");assert(!std::memcmp(&captured,&raw_material,sizeof(captured)));
    original.RestoreState(DEVSTATE_LIGHTS|DEVSTATE_MATERIAL);
    std::puts("SDK_SLOT_ORACLE raw_light_material=exact raw_enable=exact undefined_HRESULT_preserved=PASS logical_type0_capture=PASS");
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_device_state.hpp"
#include "native_surfaces.hpp"
namespace srhd_awa::matrixgame::platform3g {
// Device bindings are reset on Commit. Registered game handles, snapshots,
// surface leases and other external owners still block before any allocation.
// No state mutation or resource release occurs while deciding whether to Reset.
inline bool DefaultResourcesBlockReset(const NativeStateStore& state,
    const native_texture::Session* textures,const native_surfaces::Session* surfaces)noexcept{
    if(surfaces && surfaces->ExternalDefaultResourcesLive())return true;
    native_texture::DeviceReferences references;state.AddDeviceTextureReferences(references);
    if(surfaces)surfaces->AddDeviceTextureReferences(references);
    return references.Denied() || (textures && textures->ExternalDefaultResourcesLive(references));
}
}

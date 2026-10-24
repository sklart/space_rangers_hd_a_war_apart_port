// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "offscreen_backend.hpp"
class CTexture;
namespace srhd_awa::matrixgame::legacy_texture {
CTexture* CreateUnmanaged();
}
namespace srhd_awa::matrixgame::unmanaged_texture {
// Separate from the accepted three-field LevelDesc ABI.
struct ResourceDesc {std::uint32_t Width{},Height{},Format{},Pool{},Usage{};};
platform3g::DeviceResult Describe(IDirect3DTexture9*,std::uint32_t,ResourceDesc*);
platform3g::DeviceResult Surface(IDirect3DTexture9*,std::uint32_t,IDirect3DSurface9**);
platform3g::DeviceResult Dirty(IDirect3DTexture9*,std::nullptr_t);
platform3g::DeviceResult Update(IDirect3DTexture9*,IDirect3DTexture9*);
platform3g::DeviceResult CopySurface(IDirect3DSurface9*,IDirect3DSurface9*);
platform3g::DeviceResult DecodeBitmap(const void*,std::size_t,std::uint32_t,IDirect3DTexture9**);
}

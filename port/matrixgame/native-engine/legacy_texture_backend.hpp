// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <cstddef>
#include <string_view>
struct IDirect3DTexture9;
class CTextureManaged;
namespace srhd_awa::matrixgame::legacy_texture {
struct LockedRect {int Pitch{};void* pBits{};};
struct LevelDesc {std::uint32_t Width{},Height{},Format{};};
std::size_t RegistrySize()noexcept;
CTextureManaged* CreateManaged();
// Borrowed cache-owned entry; explicit removal follows Delete then Destroy.
CTextureManaged* GetManaged(std::wstring_view);
// These opaque objects are real SDK textures, never CPU cache entries.
void Release(IDirect3DTexture9*)noexcept;
void Preload(IDirect3DTexture9*);
std::uint32_t SetPriority(IDirect3DTexture9*,std::uint32_t);
std::uint32_t LevelCount(IDirect3DTexture9*);
platform3g::DeviceResult Describe(IDirect3DTexture9*,std::uint32_t,LevelDesc*);
platform3g::DeviceResult Lock(IDirect3DTexture9*,std::uint32_t,LockedRect*,std::nullptr_t,std::uint32_t);
platform3g::DeviceResult Unlock(IDirect3DTexture9*,std::uint32_t);
platform3g::DeviceResult Create(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**,std::uintptr_t);
platform3g::DeviceResult Decode(const void*,std::size_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
platform3g::DeviceResult DecodeFormatted(const void*,std::size_t,std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
}

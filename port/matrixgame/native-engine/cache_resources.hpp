// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Cache.hpp"
#include "texture_asset.hpp"
#include <memory>

// Original public cache lifecycle functions, implemented by CacheNative.cpp.
void CacheInit();
void CacheDeinit();
void CacheReplaceFileExt(std::wstring&,std::wstring_view,std::wstring_view);
void CacheReplaceFileNameAndExt(std::wstring&,std::wstring_view,std::wstring_view);

namespace srhd_awa::matrixgame {
std::wstring ReplaceTextureFileExt(std::wstring_view,std::wstring_view);
struct CacheTextureState;
// A borrowed material reference to a cache entry, never a D3D texture pointer.
// Cache destruction invalidates it; a returned asset lease owns its immutable
// bytes independently until the caller's current upload operation completes.
class MaterialTexture {
public:
    std::shared_ptr<const TextureAsset> Read() const;
    std::uint32_t Flags() const;
    bool Expired() const noexcept;
private:
    friend MaterialTexture BorrowMaterialTexture(CCacheData&);
    std::weak_ptr<CacheTextureState> state_;
};
CCacheData* CreateCacheTexture(CacheClass);
MaterialTexture BorrowMaterialTexture(CCacheData&);
// Size of retained decoded texture data in the cache, not GPU residency/VRAM.
std::size_t CachedTextureBytes() noexcept;
std::size_t CacheTextureEntryCount() noexcept;
bool NativeCacheFactoryBound() noexcept;
}

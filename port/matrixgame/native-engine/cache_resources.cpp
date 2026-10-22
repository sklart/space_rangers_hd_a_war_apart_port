// SPDX-License-Identifier: GPL-2.0-or-later
#include "cache_resources.hpp"
#include "texture_request.hpp"
#include "CException.hpp"
#include <stdexcept>

namespace srhd_awa::matrixgame {
namespace {std::size_t cache_bytes{},cache_entries{};}
struct CacheTextureState {
    CCacheData* owner{};
    std::shared_ptr<const TextureAsset> asset;
    std::uint32_t flags{};
    std::size_t bytes{};
};
// Explicit portable implementation of the cache resource contract. It does
// not impersonate CTextureManaged, COM, or a completed GPU texture lifecycle.
class CacheTexture final:public CCacheData {
public:
    explicit CacheTexture(CacheClass type):state_(std::make_shared<CacheTextureState>()) {
        m_Type=type;state_->owner=this;++cache_entries;
    }
    ~CacheTexture() override {ReleaseStorage();state_->owner=nullptr;--cache_entries;}
    bool IsLoaded() override {return bool(state_->asset);}
    void Load() override {
        if(IsLoaded()) {
            if(m_Type==CacheClass::Texture)throw std::logic_error("default-pool texture Load requires an unloaded resource");
            return;
        }
        auto request=TextureRequest::Resolve(m_Name);
        auto image=std::make_shared<TextureAsset>(request.Load());
        std::size_t bytes{};
        for(const auto& level:image->levels)bytes+=level.bytes.size();
        // This is a CPU retention budget. It is deliberately not a fabricated
        // GetAvailableTextureMem() result or proof that a GPU upload succeeded.
        constexpr std::size_t budget=128u*1024u*1024u;
        if(bytes>budget-cache_bytes)throw std::length_error("native cache texture budget");
        state_->flags=request.flags;state_->bytes=bytes;state_->asset=std::move(image);
        cache_bytes+=bytes;
    }
    void Unload() override {
        // The pinned USE_DX_MANAGED_TEXTURES branch intentionally retains its
        // storage on Unload; only destruction releases it. Default-pool cache
        // resources release storage immediately. Neither policy fakes a GPU.
        if(m_Type==CacheClass::TextureManaged)return;
        ReleaseStorage();
    }
    void ReleaseStorage() noexcept {
        cache_bytes-=state_->bytes;state_->bytes=0;state_->flags=0;state_->asset.reset();
    }
    std::shared_ptr<CacheTextureState> state_;
};
CCacheData* CreateCacheTexture(CacheClass type) {
    if(!NativeCacheFactoryBound())
        throw std::logic_error("UNSUPPORTED: legacy CTexture consumer has no typed native cache session");
    if(type!=CacheClass::Texture && type!=CacheClass::TextureManaged)
        throw std::invalid_argument("native cache texture class");
    return HNew(g_CacheHeap) CacheTexture(type);
}
MaterialTexture BorrowMaterialTexture(CCacheData& data) {
    auto* texture=dynamic_cast<CacheTexture*>(&data);
    if(!texture)throw std::invalid_argument("cache entry is not a portable material texture");
    MaterialTexture handle;handle.state_=texture->state_;return handle;
}
std::shared_ptr<const TextureAsset> MaterialTexture::Read() const {
    auto state=state_.lock();
    if(!state || !state->owner)throw std::logic_error("material cache owner expired");
    state->owner->Prepare();
    return state->asset;
}
std::uint32_t MaterialTexture::Flags() const {
    (void)Read();return state_.lock()->flags;
}
bool MaterialTexture::Expired() const noexcept {return state_.expired();}
std::size_t CachedTextureBytes() noexcept {return cache_bytes;}
std::size_t CacheTextureEntryCount() noexcept {return cache_entries;}
}

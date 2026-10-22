// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "cached_frame.hpp"
#include <array>
#include <map>
namespace srhd_awa::matrixgame {
// Original GSParam values; never device capabilities or a fabricated SSkin.
enum GSParam {GSP_SIDE,GSP_ORDINAL,GSP_SIDE_NOALPHA,GSP_COUNT};
struct SkinTexture {
    MaterialTexture material;
    void Preload() const {(void)material.Read();}
    std::uint32_t Flags() const {return material.Flags();}
};
struct NativeSkin {
    std::shared_ptr<SkinTexture> m_Tex,m_TexGloss,m_TexMask,m_TexBack;
    float m_tu{},m_tv{},m_dtu{},m_dtv{};
    GSParam group{GSP_ORDINAL};
    // Same original dedup key; animated UV position is intentionally excluded.
    bool operator==(const NativeSkin& sk) const {
        return m_Tex==sk.m_Tex && m_TexGloss==sk.m_TexGloss && m_TexMask==sk.m_TexMask &&
            m_TexBack==sk.m_TexBack && m_dtu==sk.m_dtu && m_dtv==sk.m_dtv;
    }
    bool Valid() const noexcept;
    void Preload() const;
private:
    friend class NativeSkinLibrary;
    std::weak_ptr<int> lifetime;
};
// Resource phase of CSkinManager, with an explicit per-session owner. Neither
// GetSkin's D3D Prepare nor the legacy SSkin callbacks are silently substituted.
class NativeSkinLibrary {
public:
    explicit NativeSkinLibrary(NativeInitRuntime& runtime,bool gloss_enabled):runtime_(runtime),gloss_enabled_(gloss_enabled){}
    ~NativeSkinLibrary(){Clear();}
    NativeSkinLibrary(const NativeSkinLibrary&)=delete;
    NativeSkinLibrary& operator=(const NativeSkinLibrary&)=delete;
    std::shared_ptr<const NativeSkin> Acquire(const wchar* textures,std::uint32_t group);
    void Takt(float cms); // Original animation/wrap algorithm, not fmod.
    void Clear() noexcept;
    std::size_t Size() const noexcept;
    std::vector<CachedFrameMaterial> SurfaceMaterials(CVectorObject&,const MeshDraw&,MeshSamplerState={},MeshRasterState={});
private:
    void Check() const;
    std::shared_ptr<SkinTexture> GetTexture(const wchar*);
    NativeInitRuntime& runtime_;
    bool gloss_enabled_;
    std::shared_ptr<int> epoch_;
    std::array<std::vector<std::shared_ptr<NativeSkin>>,GSP_COUNT> m_Skins;
    std::map<CCacheData*,std::shared_ptr<SkinTexture>> textures_;
};
CachedFrameMaterial NativeSinglePass(const NativeSkin&,std::uint32_t slot,const MeshDraw&,MeshSamplerState={},MeshRasterState={});
}

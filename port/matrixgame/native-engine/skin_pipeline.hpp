// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_skin.hpp"
namespace srhd_awa::matrixgame {
struct SkinDrawBatch {
    struct Texture {std::uint32_t slot;std::shared_ptr<const TextureAsset> asset;};
    std::vector<MeshSubmission> submissions;
    std::vector<Texture> textures;
    std::size_t stages{},set_texture{},clear{},preload{};
};
// Uses the real SSkin dispatch ABI and original VO LoadSpecial/BeforeDraw/Draw.
// Only the explicit native ordinal base shader contract is implemented here.
class NativeSkinPipeline final:public SkinProvider {
public:
    NativeSkinPipeline(NativeInitRuntime& runtime,NativeSkinLibrary& skins):runtime_(runtime),skins_(skins){}
    std::shared_ptr<const SSkin> AcquireSkin(const wchar_t*,std::uint32_t) override;
    void Load(CVectorObject&,EObjectLoad,std::uint32_t group);
    SkinDrawBatch Prepare(CVectorObject&,int frame,const MeshDraw&,
        std::shared_ptr<const SSkin> default_skin={});
private:
    NativeInitRuntime& runtime_;
    NativeSkinLibrary& skins_;
    std::map<const NativeSkin*,std::weak_ptr<const SSkin>> callbacks_;
};
}

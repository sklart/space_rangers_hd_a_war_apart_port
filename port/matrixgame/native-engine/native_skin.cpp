// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_skin.hpp"
#include "TextureFlags.hpp"
namespace srhd_awa::matrixgame {
bool NativeSkin::Valid() const noexcept {
    if(lifetime.expired() || !m_Tex)return false;
    for(const auto& texture:{m_Tex,m_TexGloss,m_TexMask,m_TexBack})
        if(texture && texture->material.Expired())return false;
    return true;
}
void NativeSkin::Preload() const {
    if(!Valid())throw std::logic_error("skin owner expired");
    for(const auto& texture:{m_Tex,m_TexGloss,m_TexMask,m_TexBack})if(texture)texture->Preload();
}
void NativeSkinLibrary::Check() const {
    if(!runtime_.CpuReady() || !g_Cache)throw std::logic_error("skin library has no cache session");
    for(const auto& [entry,texture]:textures_)
        if(texture->material.Expired())throw std::logic_error("skin cache epoch expired; Clear required");
}
std::shared_ptr<SkinTexture> NativeSkinLibrary::GetTexture(const wchar* name) {
    auto* entry=g_Cache->Get(CacheClass::TextureManaged,name);
    const auto found=textures_.find(entry);
    if(found!=textures_.end())return found->second;
    auto texture=std::make_shared<SkinTexture>(SkinTexture{BorrowMaterialTexture(*entry)});
    textures_.emplace(entry,texture);return texture;
}
void NativeSkinLibrary::Clear() noexcept {
    epoch_.reset();for(auto& skins:m_Skins)skins.clear();textures_.clear();
}
std::size_t NativeSkinLibrary::Size() const noexcept {
    std::size_t size{};for(const auto& skins:m_Skins)size+=skins.size();return size;
}
CachedFrameMaterial NativeSinglePass(const NativeSkin& skin,std::uint32_t slot,const MeshDraw& draw,
    MeshSamplerState sampling,MeshRasterState raster) {
    if(!skin.Valid())throw std::logic_error("skin owner expired");
    if(skin.group!=GSP_ORDINAL || skin.m_TexGloss || skin.m_TexMask || skin.m_TexBack)
        throw std::logic_error("UNSUPPORTED: side/gloss/mask/back skin combiner");
    if(slot==0 || slot>=64)throw std::invalid_argument("skin texture slot");
    // The original ordinal base pass enables alpha test for this flag. All
    // other lighting/sampler/raster inputs remain the explicit native contract.
    if(skin.m_Tex->Flags()&TF_ALPHATEST)raster.alpha_test=true;
    sampling.Validate();raster.Validate();
    return {skin.m_Tex->material,{draw,slot,sampling,raster}};
}
std::vector<CachedFrameMaterial> NativeSkinLibrary::SurfaceMaterials(CVectorObject& object,const MeshDraw& draw,
    MeshSamplerState sampling,MeshRasterState raster) {
    Check();if(object.m_Cache!=g_Cache)throw std::logic_error("skin VO has foreign owner");
    object.Prepare();
    if(object.NativeSurfaceCount()>63)throw std::length_error("native skin texture slots");
    std::vector<CachedFrameMaterial> result;
    for(int surface=0;surface<object.NativeSurfaceCount();++surface) {
        const auto skin=Acquire(object.GetSurfaceFileName(surface).c_str(),GSP_ORDINAL);
        result.push_back(NativeSinglePass(*skin,std::uint32_t(surface+1),draw,sampling,raster));
    }
    return result;
}
}

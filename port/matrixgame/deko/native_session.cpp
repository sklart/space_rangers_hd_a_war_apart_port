// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_session.hpp"
namespace srhd_awa::matrixgame {
NativeDekoSession::~NativeDekoSession(){Deinit();}
void NativeDekoSession::Begin(std::uint32_t width,std::uint32_t height,
    std::span<const std::byte> vertex_shader,std::span<const std::byte> fragment_shader,bool gloss_enabled) {
    if(runtime_.CpuReady() || renderer_)throw std::logic_error("native GPU session already active");
    // Begin has its own ownership/rollback guard. Do not tear down a foreign
    // session when that guard rejects entry.
    runtime_.Begin(nullptr,nullptr);
    try {
        skins_=std::make_unique<NativeSkinLibrary>(runtime_,gloss_enabled);
        pipeline_=std::make_unique<NativeSkinPipeline>(runtime_,*skins_);
        renderer_=std::make_unique<DekoMeshRenderer>(width,height,vertex_shader,fragment_shader);
        buffers_=std::make_unique<DekoBufferRenderer>(*renderer_);
    } catch(...) {Deinit();throw;}
}
void NativeDekoSession::Deinit() noexcept {
    buffers_.reset();renderer_.reset(); // Renderer waits for the real GPU queue.
    pipeline_.reset();skins_.reset();runtime_.Deinit(); // Only then destroy cached VO/texture staging.
}
CVectorObject& NativeDekoSession::LoadObject(std::wstring_view name) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    auto* object=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,std::wstring(name)));
    object->Prepare();return *object;
}
MaterialTexture NativeDekoSession::LoadTexture(std::wstring_view name) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    auto texture=BorrowMaterialTexture(*g_Cache->Get(CacheClass::TextureManaged,std::wstring(name)));
    (void)texture.Read();return texture;
}
void NativeDekoSession::Draw(CVectorObject& object,int frame,
    std::span<const CachedFrameMaterial> materials,std::span<std::byte> output) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    const auto prepared=PrepareCachedFrame(runtime_,object,frame,materials);
    for(std::size_t i=0;i<materials.size();++i)
        renderer_->SetTextureAsset(materials[i].state.texture_slot,*prepared.textures[i]);
    buffers_->Draw(buffers::Current(),prepared.submissions,output);
}
void NativeDekoSession::DrawSurfaceTextures(CVectorObject& object,int frame,const MeshDraw& draw,std::span<std::byte> output) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    const auto materials=skins_->SurfaceMaterials(object,draw);
    Draw(object,frame,materials,output);
}
void NativeDekoSession::TaktSkins(float cms) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    skins_->Takt(cms);
}
CVectorObject& NativeDekoSession::LoadSkinnedObject(std::wstring_view name,EObjectLoad flags,std::uint32_t group) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    auto* object=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,std::wstring(name)));
    pipeline_->Load(*object,flags,group);return *object;
}
void NativeDekoSession::DrawSkinned(CVectorObject& object,int frame,const MeshDraw& draw,std::span<std::byte> output) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    const auto batch=pipeline_->Prepare(object,frame,draw);
    for(const auto& texture:batch.textures)renderer_->SetTextureAsset(texture.slot,*texture.asset);
    buffers_->Draw(buffers::Current(),batch.submissions,output);
}
void NativeDekoSession::DrawAnimated(NativeVectorObjectAnim& animation,const MeshDraw& draw,std::span<std::byte> output) {
    if(!GpuReady())throw std::logic_error("native GPU session not active");
    const auto batch=pipeline_->PrepareAnimated(animation,draw);
    for(const auto& texture:batch.textures)renderer_->SetTextureAsset(texture.slot,*texture.asset);
    buffers_->Draw(buffers::Current(),batch.submissions,output);
}
}

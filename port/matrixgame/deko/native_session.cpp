// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_session.hpp"
namespace srhd_awa::matrixgame {
NativeDekoSession::~NativeDekoSession(){Deinit();}
void NativeDekoSession::Begin(std::uint32_t width,std::uint32_t height,
    std::span<const std::byte> vertex_shader,std::span<const std::byte> fragment_shader) {
    if(runtime_.CpuReady() || renderer_)throw std::logic_error("native GPU session already active");
    // Begin has its own ownership/rollback guard. Do not tear down a foreign
    // session when that guard rejects entry.
    runtime_.Begin(nullptr,nullptr);
    try {
        renderer_=std::make_unique<DekoMeshRenderer>(width,height,vertex_shader,fragment_shader);
        buffers_=std::make_unique<DekoBufferRenderer>(*renderer_);
    } catch(...) {Deinit();throw;}
}
void NativeDekoSession::Deinit() noexcept {
    buffers_.reset();renderer_.reset(); // Renderer waits for the real GPU queue.
    runtime_.Deinit(); // Only then destroy cached VO/texture staging.
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
}

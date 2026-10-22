// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../native-engine/skin_pipeline.hpp"
#include "buffer_renderer.hpp"
namespace srhd_awa::matrixgame {
// Owns an actual offscreen deko device, not the original borrowed D3D9 device.
// This does not install Win32 hooks or implement L3GRun/CGame::Init.
class NativeDekoSession {
public:
    ~NativeDekoSession();
    void Begin(std::uint32_t width,std::uint32_t height,
        std::span<const std::byte> vertex_shader,std::span<const std::byte> fragment_shader,bool gloss_enabled=false);
    void Deinit() noexcept;
    bool GpuReady() const noexcept { return bool(renderer_); }
    void Draw(CVectorObject&,int frame,std::span<const CachedFrameMaterial>,std::span<std::byte> output);
    void DrawSurfaceTextures(CVectorObject&,int frame,const MeshDraw&,std::span<std::byte> output);
    void TaktSkins(float cms);
    CVectorObject& LoadSkinnedObject(std::wstring_view name,EObjectLoad flags=OLF_AUTO,std::uint32_t group=GSP_ORDINAL);
    void DrawSkinned(CVectorObject&,int frame,const MeshDraw&,std::span<std::byte> output);
    CVectorObject& LoadObject(std::wstring_view name);
    MaterialTexture LoadTexture(std::wstring_view name);
private:
    NativeInitRuntime runtime_;
    std::unique_ptr<DekoMeshRenderer> renderer_;
    std::unique_ptr<DekoBufferRenderer> buffers_;
    std::unique_ptr<NativeSkinLibrary> skins_;
    std::unique_ptr<NativeSkinPipeline> pipeline_;
};
}

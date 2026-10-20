#include "renderer_platform.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GI_MessageLoop.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <chrono>

int main() {
  for (int cycle = 0; cycle < 10; ++cycle) {
    namespace renderer = srhd_awa::platform::renderer_platform;
    renderer::RendererConfig config;
    config.game_width = 64; config.game_height = 48; config.minimap_buffer_size = 8;
    assert(renderer::InitializeSoftwareRenderer(config, nullptr));
    auto* original = GR_Main::ScreenRenderBuffer;
    auto* original_aux = GR_Main::AuxRenderBuffer;
    original_aux->AllocateNative(64, 48);
    original_aux->FillPixels16(0xf800);
    auto pixel = [] { return *static_cast<std::uint16_t*>(GR_Main::ScreenRenderBuffer->GetPixels()); };
    original->FillPixels16(0xf800); // Unrelated preceding generation scene.
    {
        renderer::SceneCanvas scene(false);
        assert(GR_Main::ScreenRenderBuffer != original && pixel() == 0);
        assert(GR_Main::AuxRenderBuffer != original_aux);
        assert(GR_Main::AuxRenderBuffer->GetPixels() == nullptr); // No stale generation capture.
        GR_Main::ScreenRenderBuffer->FillPixels16(0x001f);
        GR_Main::CaptureScreenBackground(false, false);
        auto* scene_aux = GR_Main::AuxRenderBuffer;
        auto* parent = GR_Main::ScreenRenderBuffer;
        auto* loop = reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(std::uintptr_t{1});
        GI_MessageLoop::PushMessageLoop(loop);
        {
            renderer::SceneCanvas modal(true);
            assert(GR_Main::ScreenRenderBuffer != parent && pixel() == 0x001f);
            assert(GR_Main::AuxRenderBuffer != scene_aux);
            assert(*static_cast<std::uint16_t*>(GR_Main::AuxRenderBuffer->GetPixels()) == 0x001f);
            GR_Main::AuxRenderBuffer->FillPixels16(0x07e0);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            GR_Main::ScreenRenderBuffer->FillPixels16(0x07e0);
        }
        assert(GR_Main::ScreenRenderBuffer == parent && pixel() == 0x001f);
        assert(GR_Main::AuxRenderBuffer == scene_aux);
        assert(renderer::NestedLoopMillis() >= 2);
        assert(*static_cast<std::uint16_t*>(scene_aux->GetPixels()) == 0x001f);
        try {
            renderer::SceneCanvas modal(true);
            GR_Main::ScreenRenderBuffer->FillPixels16(0xffff);
            throw std::runtime_error("modal interrupted");
        } catch (const std::runtime_error&) {}
        assert(GR_Main::ScreenRenderBuffer == parent && pixel() == 0x001f);
        GI_MessageLoop::PopMessageLoop(loop);
    }
    assert(GR_Main::ScreenRenderBuffer == original && pixel() == 0x001f);
    assert(GR_Main::AuxRenderBuffer == original_aux);
    {
        renderer::SceneCanvas overlay(true);
        assert(pixel() == 0x001f); // Fullscreen overlay can capture OnOpen's parent.
    }
    {
        renderer::SceneCanvas next(false);
        assert(pixel() == 0); // New scene cannot inherit the previous one.
    }
    renderer::ShutdownSoftwareRenderer();
    pas::free(GI_MessageLoop::MessageLoopStack);
    GI_MessageLoop::MessageLoopStack = nullptr;
  }
  std::puts("PASS 10 cycles: actual renderer canvas isolation, parent restore and exception unwind");
}

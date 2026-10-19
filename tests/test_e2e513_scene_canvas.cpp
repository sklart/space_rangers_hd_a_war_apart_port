#include "renderer_platform.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GI_MessageLoop.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

int main() {
    namespace renderer = srhd_awa::platform::renderer_platform;
    renderer::RendererConfig config;
    config.game_width = 64; config.game_height = 48; config.minimap_buffer_size = 8;
    assert(renderer::InitializeSoftwareRenderer(config, nullptr));
    auto* original = GR_Main::ScreenRenderBuffer;
    auto pixel = [] { return *static_cast<std::uint16_t*>(GR_Main::ScreenRenderBuffer->GetPixels()); };
    original->FillPixels16(0xf800); // Unrelated preceding generation scene.
    {
        renderer::SceneCanvas scene(false);
        assert(GR_Main::ScreenRenderBuffer != original && pixel() == 0);
        GR_Main::ScreenRenderBuffer->FillPixels16(0x001f);
        auto* parent = GR_Main::ScreenRenderBuffer;
        auto* loop = reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(std::uintptr_t{1});
        GI_MessageLoop::PushMessageLoop(loop);
        {
            renderer::SceneCanvas modal(true);
            assert(GR_Main::ScreenRenderBuffer != parent && pixel() == 0x001f);
            GR_Main::ScreenRenderBuffer->FillPixels16(0x07e0);
        }
        assert(GR_Main::ScreenRenderBuffer == parent && pixel() == 0x001f);
        try {
            renderer::SceneCanvas modal(true);
            GR_Main::ScreenRenderBuffer->FillPixels16(0xffff);
            throw std::runtime_error("modal interrupted");
        } catch (const std::runtime_error&) {}
        assert(GR_Main::ScreenRenderBuffer == parent && pixel() == 0x001f);
        GI_MessageLoop::PopMessageLoop(loop);
    }
    assert(GR_Main::ScreenRenderBuffer == original && pixel() == 0x001f);
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
    std::puts("PASS actual renderer canvas isolation, parent restore and exception unwind");
}

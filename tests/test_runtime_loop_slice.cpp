#include "renderer_platform.hpp"
#include "runtime_loop_slice.hpp"
#include "units/GR_Main.hpp"

#include <cstdio>
#include <string>

int main() {
  using namespace srhd_awa::platform;
  for (int pass = 0; pass < 2; ++pass) {
    renderer_platform::RendererConfig config{};
    config.game_width = config.game_height = config.presentation_width = config.presentation_height = 64;
    config.minimap_buffer_size = 16;
    if (!renderer_platform::InitializeSoftwareRenderer(config, nullptr)) return 1;
    runtime_loop_slice::State loop;
    runtime_platform::State platform;
    std::string error;
    if (!runtime_loop_slice::Initialize(&loop, &error) || !GR_Main::InterfaceBlendPalette ||
        GR_Main::ScreenCenterX != 32 || GR_Main::ScreenCenterY != 32) return 1;
    if (!runtime_loop_slice::RunFrames(&loop, platform, 120, &error) || loop.statistics.frames != 120 ||
        loop.statistics.presents != 120) return 1;
    runtime_loop_slice::Shutdown(&loop);
    if (renderer_platform::IsInitialized() || GR_Main::InterfaceBlendPalette || GR_Main::ScreenRenderBuffer) return 1;
  }
  std::puts("M12 runtime loop regression PASS frames=120+120");
  return 0;
}
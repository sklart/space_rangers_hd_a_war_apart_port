#include "renderer_platform.hpp"

#include "units/GR_GraphBuf.hpp"
#include "units/GR_Main.hpp"

#include <cstdint>
#include <cstdio>

namespace {
constexpr std::uint64_t kGoldenFrameFnv64 = UINT64_C(0x3a9dfdc6db5aacd7);

std::uint64_t Fnv64(const void* data, std::int32_t pitch, std::int32_t width, std::int32_t height) {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  std::uint64_t value = UINT64_C(1469598103934665603);
  for (std::int32_t y = 0; y < height; ++y)
    for (std::int32_t x = 0; x < width * 2; ++x) {
      value ^= bytes[y * pitch + x];
      value *= UINT64_C(1099511628211);
    }
  return value;
}

bool DrawGoldenFrame() {
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  if (!framebuffer || framebuffer->UseTexture || !framebuffer->Pixels ||
      framebuffer->Width != 64 || framebuffer->Height != 64 || framebuffer->PitchBytes != 128 ||
      !GR_Main::RenderScratchBuffer || !GR_Main::AuxRenderBuffer || !GR_Main::CurrentPixelFormat ||
      GR_Main::CurrentPixelFormat->RedMask != 0xf800u ||
      GR_Main::CurrentPixelFormat->GreenMask != 0x07e0u ||
      GR_Main::CurrentPixelFormat->BlueMask != 0x001fu ||
      GR_Main::CurrentPixelFormat->BytesPerPixel != 2) return false;
  GR_Main::OKGR_Fill_WORD(framebuffer->GetPixels(), framebuffer->PitchBytes, 64, 64, 0x001f);
  GR_GraphBuf::TGraphBufGR_DrawHorizontalLine16(framebuffer, 3, 5, 40, 0xf800);
  GR_GraphBuf::TGraphBufGR_DrawVerticalLine16(framebuffer, 5, 3, 40, 0x07e0);
  GR_Main::LineRasterizer16(framebuffer->GetPixels(), framebuffer->PitchBytes, 6, 9, 0x00ff0000,
                             54, 21, 0x0000ff00);
  WindowsSdk::TRect clip{0, 0, 64, 64};
  GR_Main::TriangleRasterizer16(framebuffer->GetPixels(), framebuffer->PitchBytes,
                                 12, 15, 0x00ff0000, 55, 23, 0x0000ff00,
                                 30, 55, 0x000000ff, &clip);
  framebuffer->BlendPixel16(5, 5, 0xffff, 128);
  const auto* pixels = static_cast<const std::uint16_t*>(framebuffer->GetPixels());
  if (pixels[0] != 0x001f || pixels[5 + 5 * 64] == 0x001f || pixels[3 + 5 * 64] != 0xf800)
    return false;
  GR_Main::PresentWithoutLimit = true;
  if (!GR_Main::BeginFramePresentation() || !GR_Main::BeginFramePresentation()) return false;
  GR_Main::EndFramePresentation();
  if (srhd_awa::platform::renderer_platform::PresentationCount() != 0) return false;
  GR_Main::EndFramePresentation();
  const std::uint64_t hash = Fnv64(framebuffer->GetPixels(), framebuffer->PitchBytes, 64, 64);
  if (hash != kGoldenFrameFnv64 ||
      srhd_awa::platform::renderer_platform::PresentationCount() != 1 ||
      srhd_awa::platform::renderer_platform::LastPresentationHash() != hash) return false;
  std::printf("renderer golden fnv64=%016llx\n", static_cast<unsigned long long>(hash));
  return true;
}
}  // namespace

int main() {
  using srhd_awa::platform::renderer_platform::InitializeSoftwareRenderer;
  using srhd_awa::platform::renderer_platform::RendererConfig;
  using srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer;
  RendererConfig invalid{};
  invalid.game_width = 0;
  if (InitializeSoftwareRenderer(invalid, nullptr) ||
      srhd_awa::platform::renderer_platform::PresentRgb565(nullptr, 0, 0, 0)) {
    std::fputs("renderer invalid-config guard failed\n", stderr);
    return 1;
  }
  RendererConfig config{};
  config.game_width = config.game_height = config.presentation_width = config.presentation_height = 64;
  config.minimap_buffer_size = 16;
  if (!InitializeSoftwareRenderer(config, nullptr)) {
    std::fputs("renderer fixture initialization failed\n", stderr);
    return 1;
  }
  if (!DrawGoldenFrame()) {
    std::fputs("renderer golden-frame invariant failed\n", stderr);
    return 1;
  }
  ShutdownSoftwareRenderer();
  if (GR_Main::ScreenRenderBuffer || GR_Main::RenderScratchBuffer || GR_Main::AuxRenderBuffer ||
      GR_Main::CurrentPixelFormat || srhd_awa::platform::renderer_platform::IsInitialized()) {
    std::fputs("renderer shutdown cleanup failed\n", stderr);
    return 1;
  }
  GR_Main::GR_DXInit();
  const bool reinitialized = srhd_awa::platform::renderer_platform::IsInitialized() &&
      GR_Main::ScreenRenderBuffer && GR_Main::ScreenRenderBuffer->Width == 1280 &&
      GR_Main::ScreenRenderBuffer->Height == 720;
  ShutdownSoftwareRenderer();
  if (!reinitialized || srhd_awa::platform::renderer_platform::PresentRgb565(nullptr, 0, 0, 0)) {
    std::fputs("renderer default reinitialization failed\n", stderr);
    return 1;
  }
  std::puts("renderer software regression PASS");
  return 0;
}

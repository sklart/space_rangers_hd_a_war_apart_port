#include "renderer_platform.hpp"

#include "units/GR_DX.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GR_Main.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/GlobalsV.hpp"
#include "units/System.hpp"
#include "types/aMyFunction.hpp"

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#endif

#include <limits>

namespace srhd_awa::platform::renderer_platform {
namespace {
void* g_native_window{};
bool g_initialized{};
std::uint64_t g_presentation_count{};
std::uint64_t g_last_presentation_hash{};
PresentationDiagnostics g_presentation_diagnostics{};

#if defined(__SWITCH__)
SDL_Renderer* g_sdl_renderer{};
SDL_Texture* g_sdl_texture{};
std::int32_t g_texture_width{};
std::int32_t g_texture_height{};
#endif

bool ValidDimensions(std::int32_t width, std::int32_t height, std::int32_t pitch) {
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
      pitch < width * 2) return false;
  return static_cast<std::uint64_t>(pitch) * static_cast<std::uint64_t>(height) <=
         static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
}

std::uint64_t HashRgb565(const void* pixels, std::int32_t pitch, std::int32_t width,
                         std::int32_t height) {
  const auto* rows = static_cast<const std::uint8_t*>(pixels);
  // Legacy project fingerprint basis, retained for Mxx hardware evidence.
  // Standard FNV-1a-64 starts at 14695981039346656037.
  std::uint64_t value = UINT64_C(1469598103934665603);
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width * 2; ++x) {
      value ^= rows[y * pitch + x];
      value *= UINT64_C(1099511628211);
    }
  }
  return value;
}

void ClearSoftwareGlobals() {
  GR_Main::FreeScreenRenderBuffers();
  pas::free(GR_Main::CurrentPixelFormat);
  GR_Main::CurrentPixelFormat = nullptr;
  GR_DX::PendingPoints = nullptr;
  GR_DX::PendingPointCount = 0;
  GR_DX::PendingPointCapacity = 0;
  GR_Main::GameScreenWidth = 0;
  GR_Main::GameScreenHeight = 0;
  GR_Main::PresentationWidth = 0;
  GR_Main::PresentationHeight = 0;
}
}  // namespace

void SetNativeWindow(void* native_window) { g_native_window = native_window; }

bool InitializeSoftwareRenderer(const RendererConfig& config, std::string* error) {
  if (g_initialized) {
    if (error) *error = "software renderer is already initialized";
    return false;
  }
  if (config.game_width <= 0 || config.game_height <= 0 || config.game_width > 16384 ||
      config.game_height > 16384 ||
      !ValidDimensions(config.game_width, config.game_height, config.game_width * 2) ||
      config.minimap_buffer_size <= 0 || config.minimap_buffer_size > 4096) {
    if (error) *error = "invalid software renderer dimensions";
    return false;
  }
  ClearSoftwareGlobals();
  GR_Main::GameScreenWidth = config.game_width;
  GR_Main::GameScreenHeight = config.game_height;
  GR_Main::PresentationWidth = config.presentation_width > 0 ? config.presentation_width : config.game_width;
  GR_Main::PresentationHeight = config.presentation_height > 0 ? config.presentation_height : config.game_height;
  GR_Main::GameScreenRect = {0, 0, config.game_width, config.game_height};
  GR_Main::PresentationRect = {0, 0, GR_Main::PresentationWidth, GR_Main::PresentationHeight};
  GR_Main::VSyncEnabled = config.vsync;
  GR_Main::PresentWithoutLimit = !config.frame_limit;
  GR_Main::PresentationFrameRate = 60;
  GlobalsV::ScaleViewportToWindow = config.scale_viewport;
  // The Switch backend is software-only, but keeps the user request for diagnostics.
  GlobalsV::HardwareRenderingEnabled = false;

  GR_Main::CurrentPixelFormat = pas::make_object<GR_GraphBuf::TPixelFormatGR>();
  GR_Main::CurrentPixelFormat->RedMask = 0xf800u;
  GR_Main::CurrentPixelFormat->GreenMask = 0x07e0u;
  GR_Main::CurrentPixelFormat->BlueMask = 0x001fu;
  GR_Main::CurrentPixelFormat->AlphaMask = 0u;
  GR_Main::CurrentPixelFormat->BytesPerPixel = 2;
  GR_Main::CurrentPixelFormat->RebuildChannelMetrics();
  GR_Main::ScreenRenderBuffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  GR_Main::RenderScratchBuffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  GR_Main::AuxRenderBuffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  GR_Main::ScreenRenderBuffer->AllocateNativePitch(config.game_width, config.game_height,
                                                     config.game_width * 2);
  GR_Main::RenderScratchBuffer->AllocateNative(config.minimap_buffer_size, config.minimap_buffer_size);
  GR_Main::AuxRenderBuffer->AllocateNative(config.minimap_buffer_size, config.minimap_buffer_size);
  GR_Main::BlendPixel16 = GR_Main::TBlendPixel16(GR_Main::OKGR_PixelAlpha_16);
  GR_Main::TriangleRasterizer16 = GR_Main::TTriangleRasterizer16(GR_Main::OKGF_Triangle_16);
  GR_Main::LineRasterizer16 = GR_Main::TLineRasterizer16(GR_Main::OKGF_LineIp_16);
  for (std::int32_t index = 0; index <= 360; ++index) {
    const float angle = index * pas::constant(aMyFunction::GamePi / 1.8E+2L);
    GR_DX::CircleCos[index] = System::Cos(angle);
    GR_DX::CircleSin[index] = System::Sin(angle);
  }
  for (std::int32_t index = 0; index <= 359; ++index)
    GR_DX::LineAlphaTable[index] = System::Trunc(System::Cos(
        pas::real_divide(index, 1.8E+2L) * 3.14159265354L) * 127.0L + 128.0L);
  GR_DX::PendingPointCapacity = 1024;
  GR_DX::PendingPoints.set_length(GR_DX::PendingPointCapacity);
  g_initialized = true;
  return true;
}

bool InitializeReleaseCompatibleDefaults(std::string* error) {
  RendererConfig config;
  if (GR_Main::GameScreenWidth > 0) config.game_width = GR_Main::GameScreenWidth;
  if (GR_Main::GameScreenHeight > 0) config.game_height = GR_Main::GameScreenHeight;
  if (GR_Main::PresentationWidth > 0) config.presentation_width = GR_Main::PresentationWidth;
  if (GR_Main::PresentationHeight > 0) config.presentation_height = GR_Main::PresentationHeight;
  config.vsync = GR_Main::VSyncEnabled;
  config.scale_viewport = GlobalsV::ScaleViewportToWindow;
  config.frame_limit = !GR_Main::PresentWithoutLimit;
#if defined(__SWITCH__)
  if (GR_Main::GameDataConfig && GR_Main::GameDataConfig->CountParams(u"MiniMapBufSize"_wref.get()) > 0) {
    const auto size = EC_Str::ExtractDigitsToIntW(pas::view(GR_Main::GameDataConfig->GetParam(u"MiniMapBufSize"sv)));
    if (size > 0 && size <= 4096) config.minimap_buffer_size = size;
  }
#endif
  return InitializeSoftwareRenderer(config, error);
}

bool PresentRgb565(const void* pixels, std::int32_t pitch, std::int32_t width, std::int32_t height) {
  if (!g_initialized || !pixels || !ValidDimensions(width, height, pitch)) return false;
  g_last_presentation_hash = HashRgb565(pixels, pitch, width, height);
  g_presentation_diagnostics = {};
#if defined(__SWITCH__)
  if (!g_native_window) return false;
  if (!g_sdl_renderer) {
    g_sdl_renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(g_native_window), -1,
                                        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_sdl_renderer) return false;
  }
  g_presentation_diagnostics.renderer_ready = g_sdl_renderer != nullptr;
  if (!g_sdl_texture || g_texture_width != width || g_texture_height != height) {
    if (g_sdl_texture) SDL_DestroyTexture(g_sdl_texture);
    g_sdl_texture = SDL_CreateTexture(g_sdl_renderer, SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_sdl_texture) return false;
    g_texture_width = width;
    g_texture_height = height;
  }
  g_presentation_diagnostics.texture_ready = g_sdl_texture != nullptr;
  if (SDL_UpdateTexture(g_sdl_texture, nullptr, pixels, pitch) != 0) return false;
  int output_width{};
  int output_height{};
  if (SDL_GetRendererOutputSize(g_sdl_renderer, &output_width, &output_height) != 0 ||
      output_width <= 0 || output_height <= 0) return false;
  const float scale_x = static_cast<float>(output_width) / static_cast<float>(width);
  const float scale_y = static_cast<float>(output_height) / static_cast<float>(height);
  const float scale = scale_x < scale_y ? scale_x : scale_y;
  const int destination_width = static_cast<int>(static_cast<float>(width) * scale);
  const int destination_height = static_cast<int>(static_cast<float>(height) * scale);
  const SDL_Rect destination{
      (output_width - destination_width) / 2,
      (output_height - destination_height) / 2,
      destination_width,
      destination_height,
  };
  g_presentation_diagnostics.output_width = output_width;
  g_presentation_diagnostics.output_height = output_height;
  g_presentation_diagnostics.destination_x = destination.x;
  g_presentation_diagnostics.destination_y = destination.y;
  g_presentation_diagnostics.destination_width = destination.w;
  g_presentation_diagnostics.destination_height = destination.h;
  SDL_RenderClear(g_sdl_renderer);
  if (SDL_RenderCopy(g_sdl_renderer, g_sdl_texture, nullptr, &destination) != 0) return false;
  SDL_RenderPresent(g_sdl_renderer);
  g_presentation_diagnostics.present_succeeded = true;
#else
  g_presentation_diagnostics.present_succeeded = true;
#endif
  ++g_presentation_count;
  return true;
}

void ShutdownPresentation() {
#if defined(__SWITCH__)
  if (g_sdl_texture) SDL_DestroyTexture(g_sdl_texture);
  if (g_sdl_renderer) SDL_DestroyRenderer(g_sdl_renderer);
  g_sdl_texture = nullptr;
  g_sdl_renderer = nullptr;
  g_texture_width = 0;
  g_texture_height = 0;
#endif
  g_presentation_count = 0;
  g_last_presentation_hash = 0;
  g_presentation_diagnostics = {};
}

void ShutdownSoftwareRenderer() {
  ShutdownPresentation();
  if (g_initialized) ClearSoftwareGlobals();
  g_initialized = false;
}

bool IsInitialized() { return g_initialized; }
std::uint64_t PresentationCount() { return g_presentation_count; }
std::uint64_t LastPresentationHash() { return g_last_presentation_hash; }
PresentationDiagnostics LastPresentationDiagnostics() { return g_presentation_diagnostics; }

}  // namespace srhd_awa::platform::renderer_platform

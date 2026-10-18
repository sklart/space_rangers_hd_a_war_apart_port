#include "renderer_platform.hpp"
#include "e2e_clock.hpp"
#include "e2e_stage.hpp"
#include "e2e5_stage_profiler.hpp"
#include "e2e_threads.hpp"
#if defined(E2E5_HOST_SDL_WINDOW)
#include "e2e3_generation_state.hpp"
#include "units/GI_MessageLoop.hpp"
#include <array>
#include <cstdlib>
#endif

#include "units/GR_DX.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GR_Main.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/GlobalsV.hpp"
#include "units/System.hpp"
#include "types/aMyFunction.hpp"

#if defined(__SWITCH__)
#include "win32_compat_window.hpp"
#include <SDL2/SDL.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace srhd_awa::platform::renderer_platform {
namespace {
void* g_native_window{};
bool g_initialized{};
std::uint64_t g_presentation_count{};
std::uint64_t g_last_presentation_hash{};
PresentationDiagnostics g_presentation_diagnostics{};
bool g_presentation_failure_logged{};

#if defined(__SWITCH__)
SDL_Renderer* g_sdl_renderer{};
SDL_Texture* g_sdl_texture{};
std::int32_t g_texture_width{};
std::int32_t g_texture_height{};
#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
bool g_cursor_overlay_logged{};
#endif
std::uint32_t g_rate_start_tick{};
std::uint64_t g_rate_start_count{};
std::uint64_t g_present_us_sum{};
std::uint64_t g_present_us_max{};
std::uint32_t g_last_hash_tick{};

#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
bool DrawInputCursor(const SDL_Rect& destination, std::int32_t width,
                     std::int32_t height) {
  std::int32_t input_x{}, input_y{};
  win32_compat::InputCursor(&input_x, &input_y);
  const int x = destination.x + input_x * destination.w / width;
  const int y = destination.y + input_y * destination.h / height;
  // Explicit diagnostic overlay. The original game cursor is drawn in RGB565.
  const SDL_Rect shadow_vertical{x - 2, y - 12, 5, 25};
  const SDL_Rect shadow_horizontal{x - 12, y - 2, 25, 5};
  const SDL_Rect pointer_vertical{x, y - 10, 1, 21};
  const SDL_Rect pointer_horizontal{x - 10, y, 21, 1};
  const SDL_Rect pointer_center{x - 1, y - 1, 3, 3};
  if (SDL_SetRenderDrawColor(g_sdl_renderer, 0, 0, 0, 255) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &shadow_vertical) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &shadow_horizontal) != 0 ||
      SDL_SetRenderDrawColor(g_sdl_renderer, 255, 230, 0, 255) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &pointer_vertical) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &pointer_horizontal) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &pointer_center) != 0)
    return false;
  if (!g_cursor_overlay_logged) {
    e2e_stage::Log("controller cursor overlay active");
    g_cursor_overlay_logged = true;
  }
  return true;
}
#endif
#endif

bool ValidDimensions(std::int32_t width, std::int32_t height, std::int32_t pitch) {
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
      pitch < width * 2) return false;
  return static_cast<std::uint64_t>(pitch) * static_cast<std::uint64_t>(height) <=
         static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
}

bool PresentationFailure(const char* reason) {
#if defined(__SWITCH__)
  if (!g_presentation_failure_logged) {
    char detail[384]{};
    std::snprintf(detail, sizeof(detail), "present failed reason=%s sdl=%s",
                  reason, SDL_GetError());
    e2e_stage::Log(detail);
    g_presentation_failure_logged = true;
  }
#else
  static_cast<void>(reason);
#endif
  return false;
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
  GR_Main::ExtraScreenWidth = 0;
  GR_Main::ExtraScreenHeight = 0;
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
  // Mirror the original display-mode setup before UI InitializeLayout runs.
  GR_Main::ExtraScreenWidth = config.game_width - 1024;
  GR_Main::ExtraScreenHeight = config.game_height - 768;
  GR_Main::PresentationWidth = config.presentation_width > 0 ? config.presentation_width : config.game_width;
  GR_Main::PresentationHeight = config.presentation_height > 0 ? config.presentation_height : config.game_height;
  // GR_DXInit normally enumerates these modes.  The software renderer bypasses
  // that path, but Options still indexes the selected mode during OnOpen.
  const GR_Main::TDisplayModeGR fixed_mode{
      static_cast<std::uint32_t>(config.game_width),
      static_cast<std::uint32_t>(config.game_height), 60u, 0u};
  GR_Main::DesktopDisplayMode = fixed_mode;
  GR_Main::GameDisplayModes.set_length(1);
  GR_Main::RobotDisplayModes.set_length(1);
  GR_Main::GameDisplayModes[0] = fixed_mode;
  GR_Main::RobotDisplayModes[0] = fixed_mode;
  GR_Main::GameDisplayModeCount = 1;
  GR_Main::RobotDisplayModeCount = 1;
  GR_Main::SelectedGameDisplayMode = 0;
  GR_Main::SelectedRobotDisplayMode = 0;
  GR_Main::SmallestGameDisplayMode = 0;
#if defined(__SWITCH__)
  {
    char detail[128]{};
    std::snprintf(detail, sizeof(detail), "renderer geometry game=%dx%d presentation=%dx%d extra=%d,%d",
        GR_Main::GameScreenWidth, GR_Main::GameScreenHeight,
        GR_Main::PresentationWidth, GR_Main::PresentationHeight,
        GR_Main::ExtraScreenWidth, GR_Main::ExtraScreenHeight);
    e2e_stage::Log(detail);
  }
#endif
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

bool ApplyVSyncSetting(bool enabled) {
#if defined(__SWITCH__)
  if (g_sdl_renderer && SDL_RenderSetVSync(g_sdl_renderer, enabled ? 1 : 0) != 0)
    return PresentationFailure("SDL_RenderSetVSync");
#else
  (void)enabled;
#endif
  return true;
}

bool PresentRgb565(const void* pixels, std::int32_t pitch, std::int32_t width, std::int32_t height) {
  using e2e5_stage_profiler::Window;
  e2e5_stage_profiler::SetScreen(GlobalsV::CurrentScreenId);
  static Window presentation_profile{"present"};
  static std::int32_t last_screen = -1;
  static std::uint64_t previous_present_us{};
#if defined(__SWITCH__)
  if (!g_presentation_count) {
    e2e_threads::LogPriority("game-present", threadGetCurHandle(), -1, 0);
    u32 cpu_hz{};
    const Result clock_init = clkrstInitialize();
    Result clock_read = clock_init;
    if (R_SUCCEEDED(clock_init)) {
      ClkrstSession session{};
      clock_read = clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
      if (R_SUCCEEDED(clock_read)) {
        clock_read = clkrstGetClockRate(&session, &cpu_hz);
        clkrstCloseSession(&session);
      }
      clkrstExit();
    }
    char detail[192]{};
    std::snprintf(detail, sizeof(detail),
        "runtime applet_type=%u game_core=%d cpu_hz=%u clock_result=%u",
        static_cast<unsigned>(appletGetAppletType()), svcGetCurrentProcessorNumber(),
        cpu_hz, clock_read);
    e2e_stage::Log(detail);
  }
#endif
  const auto present_start_us = e2e_clock::Microseconds();
  if (!g_initialized) return PresentationFailure("renderer not initialized");
  if (!pixels) return PresentationFailure("pixels missing");
  if (!ValidDimensions(width, height, pitch)) return PresentationFailure("invalid dimensions");
#if defined(E2E5_HOST_SDL_WINDOW)
  // Explicit opt-in diagnostic capture, always from the game/UI thread.
  if (const auto* directory = std::getenv("E2E511_HOST_FRAMES")) {
    static unsigned frame_counts[256]{};
    auto screen = static_cast<unsigned>(GlobalsV::CurrentScreenId);
    if (GI_MessageLoop::MessageLoopStack && pas::list_count(GI_MessageLoop::MessageLoopStack)) {
      const auto top = pas::list_get(GI_MessageLoop::MessageLoopStack,
                                    pas::list_count(GI_MessageLoop::MessageLoopStack) - 1);
      for (unsigned id = 1; id < 42; ++id)
        if (GlobalsV::RegisteredScreens[id] == top) { screen = id; break; }
    }
    const auto frame = ++frame_counts[screen];
    if (frame == 1 || frame == 60) {
      char path[1024]{};
      std::snprintf(path, sizeof(path), "%s/screen-%u-%u.ppm", directory, screen, frame);
      if (FILE* file = std::fopen(path, "wb")) {
        std::fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (int y = 0; y < height; ++y) {
          const auto* row = reinterpret_cast<const std::uint16_t*>(
              static_cast<const unsigned char*>(pixels) + y * pitch);
          for (int x = 0; x < width; ++x) {
            const unsigned char rgb[]{static_cast<unsigned char>(((row[x] >> 11) & 31) * 255 / 31),
              static_cast<unsigned char>(((row[x] >> 5) & 63) * 255 / 63),
              static_cast<unsigned char>((row[x] & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, file);
          }
        }
        std::fclose(file);
      }
    }
  }
#endif
#if defined(__SWITCH__) && defined(E2E_DIAGNOSTICS) && !E2E_DIAGNOSTICS
  const auto hash_tick = e2e_clock::Milliseconds();
  if (!g_presentation_count || last_screen != GlobalsV::CurrentScreenId ||
      static_cast<std::uint32_t>(hash_tick - g_last_hash_tick) >= 5000u) {
    g_last_presentation_hash = HashRgb565(pixels, pitch, width, height);
    g_last_hash_tick = hash_tick;
  }
#else
  g_last_presentation_hash = HashRgb565(pixels, pitch, width, height);
#endif
#if defined(E2E5_HOST_SDL_WINDOW)
  static bool intro_captured = false;
  if (!intro_captured && GlobalsV::CurrentScreenId == 3 &&
      std::getenv("E2E511_HOST_CAPTURE_INTRO") &&
      e2e3_generation_state::g_host_continue_ready.load()) {
    // Capture the actual original UI framebuffer in this isolated host run.
    // No resource files are altered and no image is included in CI artifacts.
    if (FILE* output = std::fopen("intro-ready.ppm", "wb")) {
      std::fprintf(output, "P6\n%d %d\n255\n", width, height);
      std::array<unsigned char, 16384 * 3> row{};
      for (int y = 0; y < height; ++y) {
        auto* source = reinterpret_cast<const std::uint16_t*>(
            static_cast<const unsigned char*>(pixels) + y * pitch);
        for (int x = 0; x < width; ++x) {
          row[x * 3] = ((source[x] >> 11) & 31) * 255 / 31;
          row[x * 3 + 1] = ((source[x] >> 5) & 63) * 255 / 63;
          row[x * 3 + 2] = (source[x] & 31) * 255 / 31;
        }
        std::fwrite(row.data(), 3, width, output);
      }
      const bool ok = !std::ferror(output);
      std::fclose(output);
      intro_captured = ok;
      e2e_stage::Log(ok ? "HOST introduction framebuffer captured intro-ready.ppm" :
                         "HOST introduction framebuffer capture FAILED");
    }
  }
#endif
  presentation_profile.Measure(40, e2e_clock::Microseconds() - present_start_us);
  g_presentation_diagnostics = {};
#if defined(__SWITCH__)
  if (!g_native_window) return PresentationFailure("SDL window missing");
  if (!g_sdl_renderer) {
    g_sdl_renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(g_native_window), -1,
        SDL_RENDERER_ACCELERATED |
        (GR_Main::VSyncEnabled ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!g_sdl_renderer) return PresentationFailure("SDL_CreateRenderer");
  }
  g_presentation_diagnostics.renderer_ready = g_sdl_renderer != nullptr;
  if (!g_sdl_texture || g_texture_width != width || g_texture_height != height) {
    if (g_sdl_texture) SDL_DestroyTexture(g_sdl_texture);
    g_sdl_texture = SDL_CreateTexture(g_sdl_renderer, SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_sdl_texture) return PresentationFailure("SDL_CreateTexture");
    g_texture_width = width;
    g_texture_height = height;
  }
  g_presentation_diagnostics.texture_ready = g_sdl_texture != nullptr;
  const auto upload_start_us = e2e_clock::Microseconds();
  if (SDL_UpdateTexture(g_sdl_texture, nullptr, pixels, pitch) != 0)
    return PresentationFailure("SDL_UpdateTexture");
  presentation_profile.Measure(41, e2e_clock::Microseconds() - upload_start_us);
  const auto copy_start_us = e2e_clock::Microseconds();
  int output_width{};
  int output_height{};
  if (SDL_GetRendererOutputSize(g_sdl_renderer, &output_width, &output_height) != 0 ||
      output_width <= 0 || output_height <= 0)
    return PresentationFailure("SDL_GetRendererOutputSize");
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
  if (SDL_SetRenderDrawColor(g_sdl_renderer, 0, 0, 0, 255) != 0 ||
      SDL_RenderClear(g_sdl_renderer) != 0)
    return PresentationFailure("SDL_RenderClear");
  if (SDL_RenderCopy(g_sdl_renderer, g_sdl_texture, nullptr, &destination) != 0)
    return PresentationFailure("SDL_RenderCopy");
  presentation_profile.Measure(42, e2e_clock::Microseconds() - copy_start_us);
#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
  if (!DrawInputCursor(destination, width, height))
    return PresentationFailure("controller cursor overlay");
#endif
  const auto swap_start_us = e2e_clock::Microseconds();
  SDL_RenderPresent(g_sdl_renderer);
  presentation_profile.Measure(43, e2e_clock::Microseconds() - swap_start_us);
  g_presentation_diagnostics.present_succeeded = true;
  const auto present_us = e2e_clock::Microseconds() - present_start_us;
  g_present_us_sum += present_us;
  g_present_us_max = std::max(g_present_us_max, present_us);
#else
  g_presentation_diagnostics.present_succeeded = true;
#endif
  ++g_presentation_count;
  e2e5_stage_profiler::PresentFrame(width, height);
  const auto completed_us = e2e_clock::Microseconds();
  presentation_profile.Measure(44, completed_us - present_start_us);
  if (last_screen == GlobalsV::CurrentScreenId && previous_present_us)
    presentation_profile.Measure(45, completed_us - previous_present_us);
  if (last_screen != GlobalsV::CurrentScreenId) {
    char detail[192]{};
    std::snprintf(detail, sizeof(detail),
        "screen first presented id=%d presentations=%llu width=%d height=%d rgb565_hash=%016llx",
        GlobalsV::CurrentScreenId, static_cast<unsigned long long>(g_presentation_count),
        width, height, static_cast<unsigned long long>(g_last_presentation_hash));
    e2e_stage::Log(detail);
    last_screen = GlobalsV::CurrentScreenId;
  }
  previous_present_us = completed_us;
#if defined(E2E5_HOST_SDL_WINDOW)
  if (g_presentation_count == 1) {
    char detail[160]{};
    std::snprintf(detail, sizeof(detail),
                  "HOST first software frame width=%d height=%d rgb565_hash=%016llx",
                  width, height,
                  static_cast<unsigned long long>(g_last_presentation_hash));
    e2e_stage::Log(detail);
  }
#endif
#if defined(__SWITCH__)
  const auto now = e2e_clock::Milliseconds();
  if (!g_rate_start_tick) {
    g_rate_start_tick = now;
    g_rate_start_count = g_presentation_count;
    g_present_us_sum = 0;
    g_present_us_max = 0;
  } else if (static_cast<std::uint32_t>(now - g_rate_start_tick) >= 5000u) {
    const auto elapsed = static_cast<std::uint32_t>(now - g_rate_start_tick);
    const auto frames = g_presentation_count - g_rate_start_count;
    const auto fps_x100 = frames * UINT64_C(100000) / elapsed;
    char detail[256]{};
    std::snprintf(detail, sizeof(detail),
                  "presentation rate frames=%llu elapsed_ms=%u fps_x100=%llu present_avg_us=%llu present_max_us=%llu screen=%d rgb565_hash=%016llx",
                  static_cast<unsigned long long>(frames), elapsed,
                  static_cast<unsigned long long>(fps_x100),
                  static_cast<unsigned long long>(frames ? g_present_us_sum / frames : 0),
                  static_cast<unsigned long long>(g_present_us_max), GlobalsV::CurrentScreenId,
                  static_cast<unsigned long long>(g_last_presentation_hash));
    e2e_stage::Log(detail);
    g_rate_start_tick = now;
    g_rate_start_count = g_presentation_count;
    g_present_us_sum = 0;
    g_present_us_max = 0;
  }
#endif
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
#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
  g_cursor_overlay_logged = false;
#endif
  g_rate_start_tick = 0;
  g_rate_start_count = 0;
  g_present_us_sum = 0;
  g_present_us_max = 0;
  g_last_hash_tick = 0;
#endif
  g_presentation_count = 0;
  g_last_presentation_hash = 0;
  g_presentation_diagnostics = {};
  g_presentation_failure_logged = false;
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

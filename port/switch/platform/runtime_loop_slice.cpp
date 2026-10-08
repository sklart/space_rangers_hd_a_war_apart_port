#include "runtime_loop_slice.hpp"

#include "renderer_platform.hpp"
#include "units/EC_Mem.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GR_Main.hpp"

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::runtime_loop_slice {
namespace {
std::uint64_t NowMilliseconds() {
#if defined(__SWITCH__)
  return armGetSystemTick() / 19200ULL;
#else
  static std::uint64_t value{};
  return ++value;
#endif
}

bool ValidateFramebuffer() {
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  return framebuffer && GR_Main::CurrentPixelFormat && framebuffer->GetPixels() &&
      framebuffer->Width > 0 && framebuffer->Height > 0 && framebuffer->PitchBytes >= framebuffer->Width * 2 &&
      GR_Main::GameScreenWidth > 0 && GR_Main::GameScreenHeight > 0 &&
      GR_Main::PresentationWidth > 0 && GR_Main::PresentationHeight > 0;
}

void InitializeInterfaceBlendPalette() {
  if (GR_Main::InterfaceBlendPalette) return;
  GR_Main::InterfaceBlendPalette = EC_Mem::AllocEC(256 * 2);
  for (std::int32_t index = 0; index < 256; ++index) {
    auto* pixel = EC_Mem::AddPointerOffset(GR_Main::InterfaceBlendPalette, index * 2);
    EC_Mem::WriteWordEC(pixel, GR_Main::CurrentPixelFormat->PackRgbBytes(8, 32, 255));
    GR_Main::BlendPixel16(pixel, GR_Main::CurrentPixelFormat->PackRgbBytes(200, 128, 128), index);
  }
}

void DrawRuntimeHeartbeat(std::uint64_t frame) {
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  const auto width = framebuffer->Width;
  const auto height = framebuffer->Height;
  GR_Main::OKGR_Fill_WORD(framebuffer->GetPixels(), framebuffer->PitchBytes, width, height, 0x0010);
  const std::int32_t marker_x = static_cast<std::int32_t>(frame % static_cast<std::uint64_t>(width));
  const std::int32_t marker_y = static_cast<std::int32_t>((frame / 2) % static_cast<std::uint64_t>(height));
  GR_GraphBuf::TGraphBufGR_DrawHorizontalLine16(framebuffer, 0, marker_y, width, 0x07e0);
  GR_GraphBuf::TGraphBufGR_DrawVerticalLine16(framebuffer, marker_x, 0, height, 0xf800);
  for (std::int32_t bit = 0; bit < 16; ++bit) {
    if ((frame >> bit) & 1U) GR_GraphBuf::TGraphBufGR_DrawVerticalLine16(framebuffer, 12 + bit * 6, 12, 24, 0xffff);
  }
}

bool RunOneFrame(State* state, const runtime_platform::State& platform, std::string* error) {
  runtime_platform::PumpEvents(platform);
#if defined(__SWITCH__)
  if (!appletMainLoop()) { state->statistics.exit_reason = ExitReason::applet; return true; }
  static PadState pad;
  static bool pad_initialized{};
  if (!pad_initialized) { padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad); pad_initialized = true; }
  padUpdate(&pad);
  if (padGetButtonsDown(&pad) & HidNpadButton_Plus) { state->statistics.exit_reason = ExitReason::plus; return true; }
#endif
  if (state->exit_requested) { state->statistics.exit_reason = ExitReason::requested; return true; }
  if (state->frame_callback) {
    const std::uint64_t now_ms = NowMilliseconds();
    if (!state->frame_callback(state->frame_callback_user, now_ms, error)) {
      state->statistics.exit_reason = ExitReason::diagnostic_failure;
      return true;
    }
  }
  DrawRuntimeHeartbeat(state->statistics.frames);
  if (state->draw_callback && !state->draw_callback(state->draw_callback_user, error)) {
    state->statistics.exit_reason = ExitReason::diagnostic_failure;
    return true;
  }
  if (!GR_Main::BeginFramePresentation()) { if (error) *error = "frame presentation rejected"; return false; }
  GR_Main::EndFramePresentation();
  ++state->statistics.frames;
  state->statistics.presents = renderer_platform::PresentationCount();
#if defined(__SWITCH__)
  svcSleepThread(1000000ULL);
#endif
  return true;
}
}  // namespace

bool Initialize(State* state, std::string* error) {
  if (!state || state->initialized) { if (error) *error = "runtime loop is already initialized"; return false; }
  try {
    // The normal Switch path enters with no renderer. The host regression
    // installs a compact renderer fixture first, which is equally valid.
    if (!renderer_platform::IsInitialized()) GR_Main::GR_DXInit();
    if (!ValidateFramebuffer()) { if (error) *error = "invalid RGB565 framebuffer"; renderer_platform::ShutdownSoftwareRenderer(); return false; }
    GR_Main::ScreenCenterX = static_cast<std::uint32_t>(GR_Main::GameScreenWidth / 2);
    GR_Main::ScreenCenterY = static_cast<std::uint32_t>(GR_Main::GameScreenHeight / 2);
    InitializeInterfaceBlendPalette();
    state->started_tick = NowMilliseconds();
    state->initialized = true;
    return true;
  } catch (...) {
    if (error) *error = "portable renderer initialization failed";
    Shutdown(state);
    return false;
  }
}

void SetFrameCallback(State* state, FrameCallback callback, void* user_data) {
  if (!state) return;
  state->frame_callback = callback;
  state->frame_callback_user = user_data;
}

void SetDrawCallback(State* state, DrawCallback callback, void* user_data) {
  if (!state) return;
  state->draw_callback = callback;
  state->draw_callback_user = user_data;
}

bool RunFrames(State* state, const runtime_platform::State& platform, std::uint64_t frame_count, std::string* error) {
  if (!state || !state->initialized) { if (error) *error = "runtime loop is not initialized"; return false; }
  while (state->statistics.exit_reason == ExitReason::none && state->statistics.frames < frame_count) {
    if (!RunOneFrame(state, platform, error)) { state->statistics.exit_reason = ExitReason::presentation_failure; return false; }
  }
  if (state->statistics.exit_reason == ExitReason::none && state->statistics.frames >= frame_count) state->statistics.exit_reason = ExitReason::requested;
  state->statistics.duration_ms = NowMilliseconds() - state->started_tick;
  return state->statistics.exit_reason != ExitReason::diagnostic_failure;
}

bool RunPersistent(State* state, const runtime_platform::State& platform, std::string* error) {
  if (!state || !state->initialized) { if (error) *error = "runtime loop is not initialized"; return false; }
  while (state->statistics.exit_reason == ExitReason::none) {
    if (!RunOneFrame(state, platform, error)) { state->statistics.exit_reason = ExitReason::presentation_failure; return false; }
  }
  state->statistics.duration_ms = NowMilliseconds() - state->started_tick;
  return state->statistics.exit_reason != ExitReason::diagnostic_failure;
}

void RequestExit(State* state) { if (state) state->exit_requested = true; }
void Shutdown(State* state) {
  if (GR_Main::InterfaceBlendPalette) { EC_Mem::FreeEC(GR_Main::InterfaceBlendPalette); GR_Main::InterfaceBlendPalette = nullptr; }
  renderer_platform::ShutdownSoftwareRenderer();
  if (state) *state = {};
}
const char* ExitReasonName(ExitReason reason) {
  switch (reason) { case ExitReason::requested: return "requested"; case ExitReason::plus: return "plus"; case ExitReason::applet: return "applet"; case ExitReason::presentation_failure: return "presentation_failure"; case ExitReason::diagnostic_failure: return "diagnostic_failure"; default: return "none"; }
}
}  // namespace srhd_awa::platform::runtime_loop_slice

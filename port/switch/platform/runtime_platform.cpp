#include "runtime_platform.hpp"

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#include <switch.h>
#endif

namespace srhd_awa::platform::runtime_platform {
namespace {
std::uint32_t g_next_window_token = 1;
}

bool InitializePlatformServices(State* state, std::string* error) {
  if (!state || state->services_initialized) {
    if (error) *error = "platform state is already initialized";
    return false;
  }
#if defined(__SWITCH__)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    if (error) *error = SDL_GetError();
    return false;
  }
  state->timing_frequency = static_cast<std::int64_t>(armGetSystemTickFreq());
#else
  // CI deliberately has no native window dependency.
  state->timing_frequency = 1000000000LL;
#endif
  state->services_initialized = true;
  return true;
}

bool CreateMainWindow(State* state, std::string* error) {
  if (!state || !state->services_initialized || state->window_token != 0) {
    if (error) *error = "platform services are not ready for window creation";
    return false;
  }
#if defined(__SWITCH__)
  SDL_Window* window = SDL_CreateWindow("Space Rangers HD: A War Apart", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
  if (!window) {
    if (error) *error = SDL_GetError();
    return false;
  }
  state->native_window = window;
#endif
  state->window_token = g_next_window_token++;
  return true;
}

void PumpEvents(const State& state) {
#if defined(__SWITCH__)
  if (state.native_window) SDL_PumpEvents();
#else
  (void)state;
#endif
}

void ShutdownPlatformServices(State* state) {
  if (!state) return;
#if defined(__SWITCH__)
  if (state->native_window) SDL_DestroyWindow(static_cast<SDL_Window*>(state->native_window));
  if (state->services_initialized) SDL_Quit();
#endif
  *state = {};
}
}  // namespace srhd_awa::platform::runtime_platform

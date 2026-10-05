#pragma once

#include <cstdint>
#include <string>

namespace srhd_awa::platform::runtime_platform {

struct State {
  std::uint32_t window_token{};
  std::int64_t timing_frequency{};
  void* native_window{};
  bool services_initialized{};
};

bool InitializePlatformServices(State* state, std::string* error);
bool CreateMainWindow(State* state, std::string* error);
void PumpEvents(const State& state);
void ShutdownPlatformServices(State* state);

}  // namespace srhd_awa::platform::runtime_platform

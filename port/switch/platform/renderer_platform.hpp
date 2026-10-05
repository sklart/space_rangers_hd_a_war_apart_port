#pragma once

#include <cstdint>
#include <string>

namespace srhd_awa::platform::renderer_platform {

struct RendererConfig {
  std::int32_t game_width{1280};
  std::int32_t game_height{720};
  std::int32_t presentation_width{1280};
  std::int32_t presentation_height{720};
  std::int32_t minimap_buffer_size{156};
  bool vsync{};
  bool scale_viewport{true};
  bool frame_limit{true};
};

struct PresentationDiagnostics {
  bool renderer_ready{};
  bool texture_ready{};
  bool present_succeeded{};
  std::int32_t output_width{};
  std::int32_t output_height{};
  std::int32_t destination_x{};
  std::int32_t destination_y{};
  std::int32_t destination_width{};
  std::int32_t destination_height{};
};

void SetNativeWindow(void* native_window);
bool InitializeSoftwareRenderer(const RendererConfig& config, std::string* error);
bool InitializeReleaseCompatibleDefaults(std::string* error);
void ShutdownPresentation();
void ShutdownSoftwareRenderer();
bool PresentRgb565(const void* pixels, std::int32_t pitch, std::int32_t width,
                   std::int32_t height);
bool IsInitialized();
std::uint64_t PresentationCount();
std::uint64_t LastPresentationHash();
PresentationDiagnostics LastPresentationDiagnostics();

}  // namespace srhd_awa::platform::renderer_platform

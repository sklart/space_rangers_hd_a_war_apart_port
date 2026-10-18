// SPDX-License-Identifier: GPL-2.0-or-later
// Native boundary for the GPL MatrixGame port; no Windows DLL ABI is exposed.
#pragma once
#include <cstdint>
#include <memory>
#include <string_view>

namespace Robot { struct TRobotCallbacks; struct TRobotDisplaySettingsPrefix; }
namespace aGalaxyStruct { struct TPlanetBattleStatistics; }
namespace srhd_awa::matrixgame {
struct SoundId { std::uint32_t value{}; friend bool operator==(SoundId, SoundId) = default; };
struct ImageId { std::uint32_t value{}; friend bool operator==(ImageId, ImageId) = default; };
struct Rect { std::int32_t left{}, top{}, right{}, bottom{}; };
struct TextRequest {
  std::u16string_view text, font;
  std::uint32_t color{};
  std::int32_t width{}, height{}, align_x{}, align_y{}, wrap{}, offset_x{}, offset_y{};
  Rect clip;
};
// Pixels remain borrowed until FreeText/Reset/destruction. The owner is never
// encoded into a DWORD. The native game supplies its existing ARGB raster.
struct TextImage { ImageId id; void* pixels{}; std::int32_t pitch{}, width{}, height{}; };
struct DisplaySettings {
  void* direct3d{}; void* device{}; // Borrowed legacy reference handles, never released here.
  bool stencil_shadows{}, projected_shadows{}, selection_ex{}, terrain_gloss{}, object_gloss{}, software_cursor{};
  std::uint8_t sky{}, robot_shadow{};
  std::int32_t color_depth{}, width{}, height{}, refresh_rate{};
  float brightness{}, contrast{};
  std::int32_t fsaa_samples{}, anisotropy{};
  float max_distance{};
  bool vsync{};
};
struct BattleStatistics {
  // Preserve the native sign; MatrixGame negates the winning side's elapsed time.
  std::int32_t signed_time_ms{}, robots_built{}, robots_destroyed{}, turrets_built{}, turrets_destroyed{}, buildings_destroyed{};
};
DisplaySettings ReadSettings(const Robot::TRobotDisplaySettingsPrefix&);
BattleStatistics ReadStatistics(const aGalaxyStruct::TPlanetBattleStatistics&);
void WriteStatistics(const BattleStatistics&, aGalaxyStruct::TPlanetBattleStatistics&);

// Copies all 16 typed game callbacks. Their code and game services must outlive
// this object. Calls and ownership maps are serialized; resource releases must
// not throw, matching the main game's FreeSound/FreeText lifecycle contract.
// This is a callback adapter, not an engine backend or a fabricated battle result.
class RangersBridge {
 public:
  explicit RangersBridge(const Robot::TRobotCallbacks&);
  ~RangersBridge();
  RangersBridge(const RangersBridge&) = delete;
  RangersBridge& operator=(const RangersBridge&) = delete;
  void PlaySound(std::u16string_view);
  SoundId CreateSound(std::u16string_view, std::int32_t group, bool loop);
  void FreeSound(SoundId);
  void StartSound(SoundId);
  std::int32_t IsSoundPlaying(SoundId);
  void SetSoundVolume(SoundId, float);
  void SetSoundPan(SoundId, float);
  float GetSoundVolume(SoundId);
  float GetSoundPan(SoundId);
  TextImage RenderText(const TextRequest&);
  void FreeText(ImageId);
  void SetProgress(float);
  void PlayMusic();
  void ReleaseTextures();
  float GetMusicVolume();
  void SetMusicVolume(float);
  void Reset();
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace srhd_awa::matrixgame

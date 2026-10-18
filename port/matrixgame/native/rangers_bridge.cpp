// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_bridge.hpp"
// Load the runtime from this translation unit before entering types/. MSVC's
// nested quoted-include search would otherwise select types/classes.hpp.
#include "runtime_support.hpp"
#include "types/Robot.hpp"
#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace srhd_awa::matrixgame {
namespace {
std::atomic<std::uint64_t> next_id{1};
std::uint32_t NewId() {
  auto id = next_id.load(std::memory_order_relaxed);
  do {
    if (id > std::numeric_limits<std::uint32_t>::max())
      throw std::overflow_error("MatrixGame resource IDs exhausted");
  } while (!next_id.compare_exchange_weak(id, id + 1, std::memory_order_relaxed));
  return static_cast<std::uint32_t>(id); // An ordinal, never a native pointer.
}
std::u16string CString(std::u16string_view text) {
  if (text.find(u'\0') != text.npos) throw std::invalid_argument("embedded NUL in MatrixGame string");
  return std::u16string(text);
}
}
struct RangersBridge::Impl {
  Robot::TRobotCallbacks callbacks;
  std::recursive_mutex mutex;
  std::unordered_map<std::uint32_t, GR_Sound::TSoundBufferControl*> sounds;
  std::unordered_map<std::uint32_t, Robot::TRobotTextImage> images;
  explicit Impl(const Robot::TRobotCallbacks& value) : callbacks(value) {
    if (!pas::assigned(value.PlaySound) || !pas::assigned(value.CreateSound) ||
        !pas::assigned(value.FreeSound) || !pas::assigned(value.StartSound) ||
        !pas::assigned(value.IsSoundPlaying) || !pas::assigned(value.SetSoundVolume) ||
        !pas::assigned(value.SetSoundPan) || !pas::assigned(value.GetSoundVolume) ||
        !pas::assigned(value.GetSoundPan) || !pas::assigned(value.RenderText) ||
        !pas::assigned(value.FreeText) || !pas::assigned(value.SetProgress) ||
        !pas::assigned(value.PlayMusic) || !pas::assigned(value.ReleaseTextures) ||
        !pas::assigned(value.GetMusicVolume) || !pas::assigned(value.SetMusicVolume))
      throw std::invalid_argument("incomplete MatrixGame Rangers callbacks");
  }
  auto* Sound(SoundId id) { return id.value ? sounds.at(id.value) : nullptr; }
};
RangersBridge::RangersBridge(const Robot::TRobotCallbacks& callbacks)
    : impl_(std::make_unique<Impl>(callbacks)) {}
RangersBridge::~RangersBridge() { try { Reset(); } catch (...) {} }
void RangersBridge::PlaySound(std::u16string_view path) {
  auto text = CString(path); std::lock_guard lock(impl_->mutex);
  impl_->callbacks.PlaySound(text.data());
}
SoundId RangersBridge::CreateSound(std::u16string_view path, std::int32_t group, bool loop) {
  auto text = CString(path); const auto id = NewId(); std::lock_guard lock(impl_->mutex);
  auto* sound = impl_->callbacks.CreateSound(text.data(), group, loop ? 1 : 0);
  if (!sound) return {};
  try { impl_->sounds.emplace(id, sound); }
  catch (...) { impl_->callbacks.FreeSound(sound); throw; }
  return {id};
}
void RangersBridge::FreeSound(SoundId id) {
  std::lock_guard lock(impl_->mutex); if (!id.value) return;
  auto* sound = impl_->sounds.at(id.value);
  impl_->sounds.erase(id.value); impl_->callbacks.FreeSound(sound);
}
void RangersBridge::StartSound(SoundId id) {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.StartSound(impl_->Sound(id));
}
std::int32_t RangersBridge::IsSoundPlaying(SoundId id) {
  std::lock_guard lock(impl_->mutex); return impl_->callbacks.IsSoundPlaying(impl_->Sound(id));
}
void RangersBridge::SetSoundVolume(SoundId id, float value) {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.SetSoundVolume(impl_->Sound(id), value);
}
void RangersBridge::SetSoundPan(SoundId id, float value) {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.SetSoundPan(impl_->Sound(id), value);
}
float RangersBridge::GetSoundVolume(SoundId id) {
  std::lock_guard lock(impl_->mutex); return impl_->callbacks.GetSoundVolume(impl_->Sound(id));
}
float RangersBridge::GetSoundPan(SoundId id) {
  std::lock_guard lock(impl_->mutex); return impl_->callbacks.GetSoundPan(impl_->Sound(id));
}
TextImage RangersBridge::RenderText(const TextRequest& request) {
  auto text = CString(request.text), font = CString(request.font);
  const auto id = NewId(); std::lock_guard lock(impl_->mutex);
  Types::TRect clip{request.clip.left, request.clip.top, request.clip.right, request.clip.bottom};
  Robot::TRobotTextImage native{};
  try {
    impl_->callbacks.RenderText(text.data(), font.data(), request.color, request.width, request.height,
        request.align_x, request.align_y, request.wrap, request.offset_x, request.offset_y, request.has_clip ? &clip : nullptr, &native);
    impl_->images.emplace(id, native);
  } catch (...) { impl_->callbacks.FreeText(&native); throw; }
  return {{id}, native.Pixels, native.Pitch, native.Width, native.Height};
}
void RangersBridge::FreeText(ImageId id) {
  std::lock_guard lock(impl_->mutex); if (!id.value) return;
  auto native = impl_->images.at(id.value);
  impl_->images.erase(id.value); impl_->callbacks.FreeText(&native);
}
void RangersBridge::SetProgress(float value) {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.SetProgress(value);
}
void RangersBridge::PlayMusic() {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.PlayMusic();
}
void RangersBridge::ReleaseTextures() {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.ReleaseTextures();
}
float RangersBridge::GetMusicVolume() {
  std::lock_guard lock(impl_->mutex); return impl_->callbacks.GetMusicVolume();
}
void RangersBridge::SetMusicVolume(float value) {
  std::lock_guard lock(impl_->mutex); impl_->callbacks.SetMusicVolume(value);
}
void RangersBridge::Reset() {
  std::lock_guard lock(impl_->mutex);
  while (!impl_->images.empty()) FreeText({impl_->images.begin()->first});
  while (!impl_->sounds.empty()) FreeSound({impl_->sounds.begin()->first});
}
DisplaySettings ReadSettings(const Robot::TRobotDisplaySettingsPrefix& v) {
  return {v.Direct3D, v.Device, v.ShowStencilShadows != 0, v.ShowProjShadows != 0,
      v.SelectEx != 0, v.LandTexturesGloss != 0, v.ObjTexturesGloss != 0, v.SoftwareCursor != 0,
      v.Sky, v.RobotShadow, v.ColorDepth, v.ScreenWidth, v.ScreenHeight, v.RefreshRate,
      v.Brightness, v.Contrast, v.FSAASamples, v.Anisotropy, v.MaxDistance, v.VSync != 0};
}
BattleStatistics ReadStatistics(const aGalaxyStruct::TPlanetBattleStatistics& v) {
  return {v.SignedTimeMs, v.RobotsBuilt, v.RobotsDestroyed, v.TurretsBuilt, v.TurretsDestroyed, v.BuildingsDestroyed};
}
void WriteStatistics(const BattleStatistics& v, aGalaxyStruct::TPlanetBattleStatistics& out) {
  out.SignedTimeMs = v.signed_time_ms; out.RobotsBuilt = v.robots_built;
  out.RobotsDestroyed = v.robots_destroyed; out.TurretsBuilt = v.turrets_built;
  out.TurretsDestroyed = v.turrets_destroyed; out.BuildingsDestroyed = v.buildings_destroyed;
}
} // namespace srhd_awa::matrixgame

#pragma once

#include "gi_object.hpp"
#include "image_layout.hpp"
#include "ui_object.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace srhd_awa::package { class Package; }
namespace srhd_awa::platform::ui {

// Static release GAI instances: ordinary sequence playback, no PBuf composition.
class UiGaiLeaf final : public UiObject {
 public:
  explicit UiGaiLeaf(package::Package* package = nullptr) : animation_(package) {}
  NodeKind Kind() const override { return NodeKind::GaiLeaf; }
  void SetPackage(package::Package* package) { animation_.SetPackage(package); }
  void SetModes(image_layout::XMode x, image_layout::YMode y) { x_mode_ = x; y_mode_ = y; }
  void SetAlpha(std::uint8_t alpha) { alpha_ = alpha; }
  void SetStopAfterOneCycle(bool value) { stop_after_one_cycle_ = value; animation_.SetStopAfterOneCycle(value); }
  void SetSoundStart(std::string value) { sound_start_ = std::move(value); }
  void SetTransColor(std::string value) { trans_color_ = std::move(value); }
  void SetSkipImageUpdateRect(bool value) { skip_image_update_rect_ = value; }
  bool LoadResource(const std::string& resource, std::string* error = nullptr);
  bool LoadBytes(const std::uint8_t* bytes, std::size_t size,
                 const std::string& resource, std::string* error = nullptr);
  bool SelectEmbeddedSequence(std::int32_t index, std::string* error = nullptr) {
    return animation_.SelectEmbeddedSequence(index, error);
  }
  bool SelectCustomSequence(const std::string& sequence, std::string* error = nullptr) {
    return animation_.SelectCustomSequence(sequence, error);
  }
  bool Stop(std::string* error = nullptr) { return animation_.Stop(error); }
  bool SetFramePosition(std::int32_t index, bool forward_only = false, std::string* error = nullptr) {
    return animation_.SetFramePosition(index, forward_only, error);
  }
  bool Update(std::uint64_t delta_ms, std::string* error = nullptr);
  bool Render(const scene_compositor::Framebuffer& target, Rect clip,
              std::string* error = nullptr) const;
  bool HitTestPixel(Point point) const;
  const gi_object::GIObject& Animation() const { return animation_; }
  std::int32_t ContentOriginX() const { return animation_.Metadata().left; }
  std::int32_t ContentOriginY() const { return animation_.Metadata().top; }
  bool SkipImageUpdateRect() const { return skip_image_update_rect_; }
  const std::string& SoundStart() const { return sound_start_; }
  const std::string& TransColor() const { return trans_color_; }

 private:
  gi_object::GIObject animation_;
  image_layout::XMode x_mode_{image_layout::XMode::Center};
  image_layout::YMode y_mode_{image_layout::YMode::Center};
  std::uint8_t alpha_{255};
  bool stop_after_one_cycle_{}, skip_image_update_rect_{};
  std::string sound_start_, trans_color_;
};

}  // namespace srhd_awa::platform::ui

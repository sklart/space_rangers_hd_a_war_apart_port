#pragma once

#include "ui_object.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace srhd_awa::platform::ui {

class UiImageLeaf;
class UiLabelLeaf;

enum class GraphButtonKind { Normal, Fix, Disable, FixDisable };
enum class GraphButtonHitKind { Rect, Graph, ImageHit };
enum class GraphButtonSlot : std::uint8_t {
  Normal, NormalA, Down, DownA, Disable, DisableA, Hit, Count
};

// Images and caption remain ordinary owned UI children.  This class only
// selects state, calculates placement, and exposes a deterministic hit test.
class UiGraphButton final : public UiObject {
 public:
  NodeKind Kind() const override { return NodeKind::GraphButton; }
  bool AddStateImage(GraphButtonSlot slot, std::unique_ptr<UiImageLeaf> image,
                     std::string* error = nullptr);
  bool AddCaption(std::unique_ptr<UiLabelLeaf> caption, std::string* error = nullptr);
  UiImageLeaf* StateImage(GraphButtonSlot slot) const;
  UiLabelLeaf* Caption() const { return caption_; }

  void SetButtonKind(GraphButtonKind value);
  void SetHitKind(GraphButtonHitKind value) { hit_kind_ = value; }
  void SetHovered(bool value);
  void SetDown(bool value);
  void SetDisabled(bool value);
  void SetUpOnlyDown(bool value) { up_only_down_ = value; }
  GraphButtonKind ButtonKind() const { return button_kind_; }
  GraphButtonHitKind HitKind() const { return hit_kind_; }
  bool Hovered() const { return hovered_; }
  bool Down() const { return down_; }
  bool Disabled() const { return disabled_; }
  bool UpOnlyDown() const { return up_only_down_; }
  GraphButtonSlot VisualSlot() const { return visual_slot_; }
  bool HitTest(Point point, std::string* error = nullptr) const;

  void SetStateOffset(GraphButtonSlot slot, Point value);
  Point StateOffset(GraphButtonSlot slot) const;
  void SetCaptionOffsets(Point normal, Point down);
  void SetCaptionColors(std::array<std::uint16_t, 6> colors);
  void SetCaptionShadowColors(std::array<std::uint16_t, 6> colors);
  void SetCaptionShadowOffset(std::int32_t offset);
  void SetSoundMetadata(std::string enter, std::string leave, std::string click) {
    sound_enter_ = std::move(enter); sound_leave_ = std::move(leave); sound_click_ = std::move(click);
  }
  const std::string& SoundEnter() const { return sound_enter_; }
  const std::string& SoundLeave() const { return sound_leave_; }
  const std::string& SoundClick() const { return sound_click_; }
  void SetHasOnPressCode(bool value) { has_on_press_code_ = value; }
  bool HasOnPressCode() const { return has_on_press_code_; }
  Size MaxStateImageSize() const;
  void UpdateAutoGeometry(bool position, bool size);

 protected:
  void OnGeometryChanged() override;
  void OnAbsoluteGeometryChanged() override;

 private:
  static constexpr std::size_t Index(GraphButtonSlot slot) { return static_cast<std::size_t>(slot); }
  void UpdateVisuals();
  void PlaceStateImage(GraphButtonSlot slot);
  std::array<UiImageLeaf*, static_cast<std::size_t>(GraphButtonSlot::Count)> images_{};
  std::array<Point, static_cast<std::size_t>(GraphButtonSlot::Count)> offsets_{};
  UiLabelLeaf* caption_{};
  std::array<std::uint16_t, 6> colors_{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
  std::array<std::uint16_t, 6> shadow_colors_{};
  Point caption_normal_{}, caption_down_{};
  std::int32_t caption_shadow_offset_{};
  GraphButtonKind button_kind_{GraphButtonKind::Normal};
  GraphButtonHitKind hit_kind_{GraphButtonHitKind::Rect};
  GraphButtonSlot visual_slot_{GraphButtonSlot::Normal};
  bool hovered_{}, down_{}, disabled_{}, up_only_down_{};
  std::string sound_enter_, sound_leave_, sound_click_;
  bool has_on_press_code_{};
};

}  // namespace srhd_awa::platform::ui

#pragma once

#include "ui_object.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace srhd_awa::platform::ui {

class UiImageLeaf;

enum class ScrollBarPart : std::uint8_t {
  Up, Before, ThumbTop, ThumbCenter, ThumbBottom, After, Down, Count
};
enum class ScrollBarState : std::uint8_t { Normal, Active, Down, Count };

class UiScrollBar final : public UiPanel {
 public:
  NodeKind Kind() const override { return NodeKind::ScrollBar; }
  bool AddImage(ScrollBarPart part, ScrollBarState state,
                std::unique_ptr<UiImageLeaf> image, std::string* error = nullptr);
  UiImageLeaf* Image(ScrollBarPart part, ScrollBarState state) const;
  void SetRange(std::int32_t minimum, std::int32_t maximum);
  void SetPosition(std::int32_t position);
  void SetPositionInternal(std::int32_t position);
  void SetPositionChangedCallback(std::function<void(std::int32_t)> callback) {
    position_changed_ = std::move(callback);
  }
  void SetSmallChange(std::int32_t value) { small_change_ = value; }
  void SetLargeChange(std::int32_t value);
  void SetPageSize(std::int32_t value);
  void SetOrientation(std::int32_t value);
  void UpdateSizeForOrientation();
  void SetCalculationMode(std::int32_t value);
  void SetHoveredRegion(std::int32_t value);
  void SetPressedRegion(std::int32_t value);
  std::int32_t GetHitRegion(Point local) const;
  void PressRegion(std::int32_t value);
  void ReleaseRegion();
  bool UpdateLayout(std::string* error = nullptr);

  std::int32_t Minimum() const { return minimum_; }
  std::int32_t Maximum() const { return maximum_; }
  std::int32_t Position() const { return position_; }
  std::int32_t SmallChange() const { return small_change_; }
  std::int32_t LargeChange() const { return large_change_; }
  std::int32_t PageSize() const { return page_size_; }
  std::int32_t Orientation() const { return orientation_; }
  std::int32_t CalculationMode() const { return calculation_mode_; }
  std::int32_t HoveredRegion() const { return hovered_region_; }
  std::int32_t PressedRegion() const { return pressed_region_; }
  std::int32_t TrackLength() const { return track_length_; }
  std::int32_t ThumbLength() const { return thumb_length_; }
  std::int32_t BeforeLength() const { return before_length_; }
  std::int32_t AfterLength() const { return after_length_; }
  bool Narrow() const { return narrow_; }

 protected:
  void OnGeometryChanged() override { UpdateLayout(); }

 private:
  static constexpr std::size_t Index(ScrollBarPart part, ScrollBarState state) {
    return static_cast<std::size_t>(part) * 3 + static_cast<std::size_t>(state);
  }
  void Place(ScrollBarPart part, Point position, Size size, bool visible);
  std::int32_t AxisSize(ScrollBarPart part) const;
  std::int32_t CrossSize(ScrollBarPart part) const;
  std::int32_t ClampPosition(std::int32_t value) const;
  std::array<UiImageLeaf*, 21> images_{};
  std::int32_t minimum_{}, maximum_{99}, position_{};
  std::int32_t large_change_{1}, small_change_{1}, page_size_{1};
  std::int32_t orientation_{2}, calculation_mode_{};
  std::int32_t hovered_region_{}, pressed_region_{};
  std::int32_t track_length_{}, thumb_length_{}, before_length_{}, after_length_{};
  bool narrow_{};
  std::function<void(std::int32_t)> position_changed_;
};

}  // namespace srhd_awa::platform::ui

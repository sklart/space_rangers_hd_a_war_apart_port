#include "ui_scroll_bar.hpp"

#include "ui_tree_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
std::int32_t Bounded(std::int64_t value) {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(value,
      std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}
std::int32_t RegionFor(ScrollBarPart part) {
  switch (part) {
    case ScrollBarPart::Up: return 1;
    case ScrollBarPart::Down: return 2;
    case ScrollBarPart::Before: return 3;
    case ScrollBarPart::After: return 4;
    default: return 5;
  }
}
}  // namespace

UiScrollBar* UiObject::AddScrollBar() {
  auto child = std::make_unique<UiScrollBar>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

UiImageLeaf* UiScrollBar::Image(ScrollBarPart part, ScrollBarState state) const {
  const auto index = Index(part, state);
  return index < images_.size() ? images_[index] : nullptr;
}

bool UiScrollBar::AddImage(ScrollBarPart part, ScrollBarState state,
                           std::unique_ptr<UiImageLeaf> image, std::string* error) {
  const auto index = Index(part, state);
  if (index >= images_.size() || !image || images_[index])
    return Fail(error, "invalid or duplicate ScrollBar image");
  image->SetDepth(part >= ScrollBarPart::ThumbTop && part <= ScrollBarPart::ThumbBottom
                      ? 0.0 : 1.0);
  image->SetActive(false);
  auto* owned = image.get();
  if (!Attach(std::move(image), error)) return false;
  images_[index] = owned;
  return true;
}

std::int32_t UiScrollBar::AxisSize(ScrollBarPart part) const {
  const auto* image = Image(part, ScrollBarState::Normal);
  if (!image) return 0;
  return orientation_ == 1 ? image->ClientSize().width : image->ClientSize().height;
}
std::int32_t UiScrollBar::CrossSize(ScrollBarPart part) const {
  const auto* image = Image(part, ScrollBarState::Normal);
  if (!image) return 0;
  return orientation_ == 1 ? image->ClientSize().height : image->ClientSize().width;
}

std::int32_t UiScrollBar::ClampPosition(std::int32_t value) const {
  const auto upper = calculation_mode_ == 0 ? static_cast<std::int64_t>(maximum_) :
      std::max<std::int64_t>(minimum_, static_cast<std::int64_t>(maximum_) - page_size_ + 1);
  return Bounded(std::clamp<std::int64_t>(value, minimum_, upper));
}
void UiScrollBar::SetRange(std::int32_t minimum, std::int32_t maximum) {
  minimum_ = std::min(minimum, maximum);
  maximum_ = maximum;
  position_ = ClampPosition(position_);
  if (static_cast<std::int64_t>(maximum_) - minimum_ + 1 < page_size_)
    position_ = minimum_;
  UpdateLayout();
}
void UiScrollBar::SetPosition(std::int32_t position) {
  const auto before = position_;
  SetPositionInternal(position);
  if (position_ != before && Active() && position_changed_)
    position_changed_(position_);
}
void UiScrollBar::SetPositionInternal(std::int32_t position) {
  position_ = ClampPosition(position);
  UpdateLayout();
}
void UiScrollBar::SetLargeChange(std::int32_t value) {
  // Upstream compares against SmallChange, even when LargeChange differs.
  if (small_change_ != value) large_change_ = value;
}
void UiScrollBar::SetPageSize(std::int32_t value) {
  page_size_ = Bounded(std::min<std::int64_t>(value,
      static_cast<std::int64_t>(maximum_) - minimum_ + 1));
  UpdateLayout();
}
void UiScrollBar::SetOrientation(std::int32_t value) {
  orientation_ = value;
  UpdateLayout();
}
void UiScrollBar::UpdateSizeForOrientation() {
  const auto* image = Image(ScrollBarPart::Up, ScrollBarState::Normal);
  if (!image) return;
  const auto size = ClientSize();
  if (orientation_ == 1) UiObject::SetSize({size.width, image->ClientSize().height});
  else UiObject::SetSize({image->ClientSize().width, size.height});
}
void UiScrollBar::SetCalculationMode(std::int32_t value) {
  calculation_mode_ = value;
  UpdateLayout();
}
void UiScrollBar::SetHoveredRegion(std::int32_t value) {
  hovered_region_ = value >= 0 && value <= 5 ? value : 0;
  UpdateLayout();
}
void UiScrollBar::SetPressedRegion(std::int32_t value) {
  pressed_region_ = value >= 0 && value <= 5 ? value : 0;
  UpdateLayout();
}

void UiScrollBar::Place(ScrollBarPart part, Point position, Size size, bool visible) {
  const auto region = RegionFor(part);
  const auto state = pressed_region_ == region ? ScrollBarState::Down :
      (pressed_region_ == 0 && hovered_region_ == region ? ScrollBarState::Active :
                                                          ScrollBarState::Normal);
  for (std::uint8_t i = 0; i < 3; ++i) {
    auto* image = Image(part, static_cast<ScrollBarState>(i));
    if (!image) continue;
    image->SetPosition(position);
    image->SetSize(size);
    image->SetActive(visible && i == static_cast<std::uint8_t>(state));
  }
}

bool UiScrollBar::UpdateLayout(std::string* error) {
  const auto client = ClientSize();
  const bool horizontal = orientation_ == 1;
  const auto length = horizontal ? client.width : client.height;
  const auto cross = horizontal ? client.height : client.width;
  if (length < 0 || cross < 0 || length > 16384 || cross > 16384)
    return Fail(error, "ScrollBar geometry exceeds safe bounds");
  const auto up = AxisSize(ScrollBarPart::Up), down = AxisSize(ScrollBarPart::Down);
  const auto top = AxisSize(ScrollBarPart::ThumbTop);
  const auto bottom = AxisSize(ScrollBarPart::ThumbBottom);
  if (up < 0 || down < 0 || top < 0 || bottom < 0 ||
      up > 16384 || down > 16384 || top > 16384 || bottom > 16384)
    return Fail(error, "ScrollBar image geometry is invalid");
  const auto place = [&](ScrollBarPart part, std::int32_t axis, std::int32_t axis_size,
                         bool visible = true) {
    const auto cross_size = CrossSize(part);
    const auto cross_pos = (cross - cross_size) / 2;
    const Point point = horizontal ? Point{axis, cross_pos} : Point{cross_pos, axis};
    const Size size = horizontal ? Size{axis_size, cross_size} : Size{cross_size, axis_size};
    Place(part, point, size, visible);
  };
  track_length_ = thumb_length_ = before_length_ = after_length_ = 0;
  narrow_ = static_cast<std::int64_t>(up) + down + top + bottom >= length;
  if (narrow_) {
    place(ScrollBarPart::Up, 0, up);
    place(ScrollBarPart::Before, up, AxisSize(ScrollBarPart::Before));
    place(ScrollBarPart::ThumbTop, up, top);
    place(ScrollBarPart::ThumbCenter, up + top, 0, false);
    place(ScrollBarPart::After, up + top, AxisSize(ScrollBarPart::After));
    place(ScrollBarPart::ThumbBottom, up + top, bottom);
    place(ScrollBarPart::Down, up + top + bottom, down);
    if (error) error->clear();
    return true;
  }
  track_length_ = length - up - down;
  const auto range = static_cast<std::int64_t>(maximum_) - minimum_ + 1;
  const auto span = static_cast<std::int64_t>(position_) - minimum_;
  thumb_length_ = range == 0 ? track_length_ :
      Bounded(static_cast<std::int64_t>(page_size_) * track_length_ / range);
  thumb_length_ = std::min(thumb_length_, track_length_);
  const auto minimum_thumb = top + bottom >= thumb_length_ ? top + bottom : 0;
  if (minimum_thumb) thumb_length_ = minimum_thumb;
  if (calculation_mode_ == 0) {
    if (!minimum_thumb)
      before_length_ = range == 0 ? 0 : Bounded(span * track_length_ / range);
    else {
      const auto denominator = static_cast<std::int64_t>(maximum_) - minimum_;
      before_length_ = denominator == 0 ? 0 :
          Bounded(span * (track_length_ - minimum_thumb) / denominator);
    }
  } else {
    before_length_ = range == 0 ? 0 : Bounded(span * track_length_ / range);
    if (before_length_ + thumb_length_ > track_length_)
      before_length_ = track_length_ - thumb_length_;
  }
  after_length_ = track_length_ - before_length_ - thumb_length_;
  place(ScrollBarPart::Up, 0, up);
  place(ScrollBarPart::Down, length - down, down);
  place(ScrollBarPart::ThumbTop, up + before_length_, top);
  place(ScrollBarPart::ThumbBottom, up + before_length_ + thumb_length_ - bottom, bottom);
  place(ScrollBarPart::ThumbCenter, up + top + before_length_,
        thumb_length_ - top - bottom, thumb_length_ > top + bottom);
  // These two widths include the adjacent thumb cap in upstream UpdateLayout.
  place(ScrollBarPart::Before, up, top + before_length_);
  place(ScrollBarPart::After, before_length_ + thumb_length_ - bottom + down,
        bottom + after_length_);
  if (error) error->clear();
  return true;
}

std::int32_t UiScrollBar::GetHitRegion(Point local) const {
  const auto axis = orientation_ == 1 ? local.x : local.y;
  const auto length = orientation_ == 1 ? ClientSize().width : ClientSize().height;
  const auto up_end = AxisSize(ScrollBarPart::Up);
  const auto top_image = Image(ScrollBarPart::ThumbTop, ScrollBarState::Normal);
  const auto bottom_image = Image(ScrollBarPart::ThumbBottom, ScrollBarState::Normal);
  const auto thumb_start = top_image ?
      (orientation_ == 1 ? top_image->LocalPosition().x : top_image->LocalPosition().y) : 0;
  const auto thumb_end = bottom_image ?
      (orientation_ == 1 ? bottom_image->LocalPosition().x : bottom_image->LocalPosition().y) +
          AxisSize(ScrollBarPart::ThumbBottom) : 0;
  const auto down_start = length - AxisSize(ScrollBarPart::Down);
  if (axis < 0) return 0;
  if (axis <= up_end) return 1;
  if (axis <= thumb_start) return 3;
  if (axis <= thumb_end) return 5;
  if (axis <= down_start) return 4;
  return axis <= length ? 2 : 0;
}

void UiScrollBar::PressRegion(std::int32_t value) {
  SetPressedRegion(value);
  const auto step = value == 1 || value == 2 ? small_change_ : large_change_;
  if (value == 1 || value == 3)
    SetPosition(Bounded(static_cast<std::int64_t>(position_) - step));
  else if (value == 2 || value == 4)
    SetPosition(Bounded(static_cast<std::int64_t>(position_) + step));
}
void UiScrollBar::ReleaseRegion() { SetPressedRegion(0); }

}  // namespace srhd_awa::platform::ui

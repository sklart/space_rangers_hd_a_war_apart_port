#include "ui_graph_button.hpp"

#include "ui_label.hpp"
#include "ui_tree_renderer.hpp"

#include <algorithm>
#include <limits>
#include <memory>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
constexpr GraphButtonSlot kVisibleSlots[] = {
    GraphButtonSlot::Normal, GraphButtonSlot::NormalA, GraphButtonSlot::Down,
    GraphButtonSlot::DownA, GraphButtonSlot::Disable, GraphButtonSlot::DisableA};
}  // namespace

UiGraphButton* UiObject::AddGraphButton() {
  auto child = std::make_unique<UiGraphButton>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

UiImageLeaf* UiGraphButton::StateImage(GraphButtonSlot slot) const {
  const auto index = Index(slot);
  return index < images_.size() ? images_[index] : nullptr;
}

bool UiGraphButton::AddStateImage(GraphButtonSlot slot, std::unique_ptr<UiImageLeaf> image,
                                  std::string* error) {
  const auto index = Index(slot);
  if (index >= images_.size() || !image || images_[index])
    return Fail(error, "invalid or duplicate GraphButton image slot");
  image->SetDepth(1.0);
  image->SetActive(false);
  auto* owned = image.get();
  if (!Attach(std::move(image), error)) return false;
  images_[index] = owned;
  PlaceStateImage(slot);
  UpdateVisuals();
  return true;
}

bool UiGraphButton::AddCaption(std::unique_ptr<UiLabelLeaf> caption, std::string* error) {
  if (!caption || caption_) return Fail(error, "GraphButton caption already exists");
  caption->SetDepth(-9999.0);
  auto* owned = caption.get();
  if (!Attach(std::move(caption), error)) return false;
  caption_ = owned;
  OnGeometryChanged();
  UpdateVisuals();
  return true;
}

void UiGraphButton::SetButtonKind(GraphButtonKind value) {
  if (button_kind_ == value) return;
  button_kind_ = value;
  UpdateVisuals();
}
void UiGraphButton::SetHovered(bool value) {
  if (hovered_ == value) return;
  hovered_ = value;
  UpdateVisuals();
}
void UiGraphButton::SetDown(bool value) {
  if (down_ == value) return;
  down_ = value;
  UpdateVisuals();
}
void UiGraphButton::SetDisabled(bool value) {
  if (disabled_ == value) return;
  disabled_ = value;
  UpdateVisuals();
}

void UiGraphButton::UpdateVisuals() {
  if (disabled_ && (button_kind_ == GraphButtonKind::Disable ||
                    button_kind_ == GraphButtonKind::FixDisable)) {
    visual_slot_ = hovered_ && StateImage(GraphButtonSlot::DisableA)
                       ? GraphButtonSlot::DisableA : GraphButtonSlot::Disable;
  } else if (down_) {
    visual_slot_ = hovered_ && StateImage(GraphButtonSlot::DownA)
                       ? GraphButtonSlot::DownA : GraphButtonSlot::Down;
  } else {
    visual_slot_ = hovered_ && StateImage(GraphButtonSlot::NormalA)
                       ? GraphButtonSlot::NormalA : GraphButtonSlot::Normal;
  }
  for (const auto slot : kVisibleSlots)
    if (auto* image = StateImage(slot)) image->SetActive(slot == visual_slot_);
  if (auto* hit = StateImage(GraphButtonSlot::Hit)) hit->SetActive(false);
  if (caption_) {
    const auto index = Index(visual_slot_);
    caption_->SetTextColor(colors_[index]);
    caption_->SetTextShadow(caption_shadow_offset_, shadow_colors_[index]);
    caption_->SetPosition(down_ ? caption_down_ : caption_normal_);
  }
}

bool UiGraphButton::HitTest(Point point, std::string* error) const {
  if (hit_kind_ == GraphButtonHitKind::Rect) return ContainsPoint(point);
  const auto test_image = [&](UiImageLeaf* leaf) {
    return leaf && leaf->Image().HitTest(point.x, point.y, error);
  };
  if (hit_kind_ == GraphButtonHitKind::ImageHit)
    return test_image(StateImage(GraphButtonSlot::Hit));
  for (const auto slot : kVisibleSlots)
    if (test_image(StateImage(slot))) return true;
  return false;
}

void UiGraphButton::PlaceStateImage(GraphButtonSlot slot) {
  auto* image = StateImage(slot);
  if (!image) return;
  image->SetPosition(offsets_[Index(slot)]);
  const auto bounds = image->HitTestBounds();
  image->Image().SetPosition(bounds.left, bounds.top);
  image->Image().SetOrigin(0, 0);
  image->Image().SetSize(image->ClientSize().width, image->ClientSize().height);
}
void UiGraphButton::OnGeometryChanged() {
  for (std::size_t index{}; index < images_.size(); ++index)
    PlaceStateImage(static_cast<GraphButtonSlot>(index));
  if (caption_) caption_->SetSize(ClientSize());
}
void UiGraphButton::OnAbsoluteGeometryChanged() {
  for (std::size_t index{}; index < images_.size(); ++index)
    PlaceStateImage(static_cast<GraphButtonSlot>(index));
}
void UiGraphButton::SetStateOffset(GraphButtonSlot slot, Point value) {
  const auto index = Index(slot);
  if (index >= offsets_.size()) return;
  offsets_[index] = value;
  PlaceStateImage(slot);
}
Point UiGraphButton::StateOffset(GraphButtonSlot slot) const {
  const auto index = Index(slot);
  return index < offsets_.size() ? offsets_[index] : Point{};
}
void UiGraphButton::SetCaptionOffsets(Point normal, Point down) {
  caption_normal_ = normal;
  caption_down_ = down;
  UpdateVisuals();
}
void UiGraphButton::SetCaptionColors(std::array<std::uint16_t, 6> colors) {
  colors_ = colors;
  UpdateVisuals();
}
void UiGraphButton::SetCaptionShadowColors(std::array<std::uint16_t, 6> colors) {
  shadow_colors_ = colors;
  UpdateVisuals();
}
void UiGraphButton::SetCaptionShadowOffset(std::int32_t offset) {
  caption_shadow_offset_ = offset;
  UpdateVisuals();
}
Size UiGraphButton::MaxStateImageSize() const {
  Size result{};
  for (const auto* image : images_) {
    if (!image) continue;
    result.width = std::max(result.width, image->Image().natural_width());
    result.height = std::max(result.height, image->Image().natural_height());
  }
  return result;
}
void UiGraphButton::UpdateAutoGeometry(bool position, bool size) {
  Rect bounds{};
  bool found = false;
  for (const auto slot : kVisibleSlots) {
    const auto* image = StateImage(slot);
    if (!image) continue;
    const auto offset = StateOffset(slot);
    const Rect image_bounds{offset.x, offset.y,
                            offset.x + image->Image().natural_width(),
                            offset.y + image->Image().natural_height()};
    bounds = found ? Union(bounds, image_bounds) : image_bounds;
    found = true;
  }
  if (!found) return;
  if (position) SetPosition({LocalPosition().x + bounds.left, LocalPosition().y + bounds.top});
  if (size) SetSize({bounds.right - bounds.left, bounds.bottom - bounds.top});
}

}  // namespace srhd_awa::platform::ui

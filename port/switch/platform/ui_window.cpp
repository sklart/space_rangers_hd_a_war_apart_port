#include "ui_window.hpp"

#include "ui_tree_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
}  // namespace

UiWindow* UiObject::AddWindow() {
  auto child = std::make_unique<UiWindow>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

UiImageLeaf* UiWindow::BorderImage(WindowSlot slot) const {
  const auto index = Index(slot);
  return index < images_.size() ? images_[index] : nullptr;
}

bool UiWindow::HasAllImages() const {
  return std::all_of(images_.begin(), images_.end(), [](const auto* image) { return image != nullptr; });
}

bool UiWindow::AddBorderImage(WindowSlot slot, std::unique_ptr<UiImageLeaf> image,
                              std::string* error) {
  const auto index = Index(slot);
  if (index >= images_.size() || !image || images_[index])
    return Fail(error, "invalid or duplicate Window image slot");
  image->SetDepth(1.0e6);
  auto* owned = image.get();
  if (!Attach(std::move(image), error)) return false;
  images_[index] = owned;
  if (HasAllImages()) Layout();
  return true;
}

bool UiWindow::AlignSizeToBorderTiles(Size requested, Size* aligned, std::string* error) const {
  if (!aligned || !HasAllImages()) return Fail(error, "Window border images are incomplete");
  const auto natural = [&](WindowSlot slot) -> Size {
    const auto& image = BorderImage(slot)->Image();
    return {image.natural_width(), image.natural_height()};
  };
  const auto top_left = natural(WindowSlot::TopLeft);
  const auto top_right = natural(WindowSlot::TopRight);
  const auto bottom_left = natural(WindowSlot::BottomLeft);
  const auto top = natural(WindowSlot::Top);
  const auto left = natural(WindowSlot::Left);
  const auto width = std::max(requested.width, minimum_size_.width);
  const auto height = std::max(requested.height, minimum_size_.height);
  const auto border_width = std::int64_t(top_left.width) + top_right.width;
  const auto border_height = std::int64_t(top_left.height) + bottom_left.height;
  auto aligned_width = border_width;
  auto aligned_height = border_height;
  if (width > border_width) {
    if (top.width <= 0) return Fail(error, "Window top tile width is zero");
    const auto interior = std::int64_t(width) - border_width;
    aligned_width += ((interior + top.width - 1) / top.width) * top.width;
  }
  if (height > border_height) {
    if (left.height <= 0) return Fail(error, "Window left tile height is zero");
    const auto interior = std::int64_t(height) - border_height;
    aligned_height += ((interior + left.height - 1) / left.height) * left.height;
  }
  if (aligned_width < 0 || aligned_height < 0 ||
      aligned_width > std::numeric_limits<std::int32_t>::max() ||
      aligned_height > std::numeric_limits<std::int32_t>::max())
    return Fail(error, "Window aligned size overflows");
  *aligned = {static_cast<std::int32_t>(aligned_width), static_cast<std::int32_t>(aligned_height)};
  return true;
}

bool UiWindow::FinalizeLayout(std::string* error) {
  if (!HasAllImages()) return Fail(error, "Window border images are incomplete");
  for (const auto* image : images_)
    if (image->Image().natural_width() <= 0 || image->Image().natural_height() <= 0)
      return Fail(error, "Window border image has zero natural size");
  Size aligned{};
  if (!AlignSizeToBorderTiles(ClientSize(), &aligned, error)) return false;
  SetSize(aligned);
  Layout();
  return true;
}

void UiWindow::OnGeometryChanged() {
  if (HasAllImages()) Layout();
}

void UiWindow::Layout() {
  const auto natural = [&](WindowSlot slot) -> Size {
    const auto& image = BorderImage(slot)->Image();
    return {image.natural_width(), image.natural_height()};
  };
  const auto place = [&](WindowSlot slot, Point position, Size size, bool active,
                         image_layout::XMode x = image_layout::XMode::Center,
                         image_layout::YMode y = image_layout::YMode::Center) {
    auto* image = BorderImage(slot);
    image->SetSize(size);
    image->SetPosition(position);
    image->Image().SetModes(x, y);
    image->SetActive(active);
  };
  const auto window = ClientSize();
  const auto tl = natural(WindowSlot::TopLeft);
  const auto tr = natural(WindowSlot::TopRight);
  const auto bl = natural(WindowSlot::BottomLeft);
  const auto br = natural(WindowSlot::BottomRight);
  const auto top = natural(WindowSlot::Top);
  const auto bottom = natural(WindowSlot::Bottom);
  const auto left = natural(WindowSlot::Left);
  const auto right = natural(WindowSlot::Right);
  place(WindowSlot::TopLeft, {0, 0}, tl, true);
  place(WindowSlot::TopRight, {window.width - tr.width, 0}, tr, true);
  place(WindowSlot::BottomLeft, {0, window.height - bl.height}, bl, true);
  place(WindowSlot::BottomRight, {window.width - br.width, window.height - br.height}, br, true);
  const auto top_width = window.width - tl.width - tr.width;
  const auto bottom_width = window.width - bl.width - br.width;
  place(WindowSlot::Top, {tl.width, 0}, {std::max(0, top_width), top.height}, top_width > 0,
        image_layout::XMode::LeftFill);
  place(WindowSlot::Bottom, {bl.width, window.height - bottom.height},
        {std::max(0, bottom_width), bottom.height}, bottom_width > 0,
        image_layout::XMode::LeftFill);
  const auto left_height = window.height - tl.height - bl.height;
  const auto right_height = window.height - tr.height - br.height;
  place(WindowSlot::Left, {0, tl.height}, {left.width, std::max(0, left_height)}, left_height > 0,
        image_layout::XMode::Center, image_layout::YMode::TopFill);
  place(WindowSlot::Right, {window.width - right.width, tr.height},
        {right.width, std::max(0, right_height)}, right_height > 0,
        image_layout::XMode::Center, image_layout::YMode::TopFill);
  const auto interior_width = window.width - left.width - right.width;
  const auto interior_height = window.height - top.height - bottom.height;
  place(WindowSlot::Texture, {left.width, top.height},
        {std::max(0, interior_width), std::max(0, interior_height)},
        interior_width > 0 && interior_height > 0,
        image_layout::XMode::LeftFill, image_layout::YMode::TopFill);
}

}  // namespace srhd_awa::platform::ui

#include "ui_graph_buffer.hpp"

namespace srhd_awa::platform::ui {
namespace {
okgf_rle_bridge::Rect Convert(Rect value) {
  return {value.left, value.top, value.right, value.bottom};
}
}

UiGraphBuffer* UiObject::AddGraphBuffer() {
  auto child = std::make_unique<UiGraphBuffer>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

bool UiGraphBuffer::RenderLeaf(const scene_compositor::Framebuffer& target, Rect clip,
                               std::string* error) const {
  if (!buffer_.owns_buffer()) return true;
  return buffer_.Draw(target, Convert(HitTestBounds()), Convert(clip),
                      x_mode_, y_mode_, half_alpha_, error);
}

bool UiGraphBuffer::HitTestPixel(Point point) const {
  return Active() && buffer_.HitTestPixel(point.x, point.y,
      Convert(HitTestBounds()), Convert(HitTestBounds()), x_mode_, y_mode_, half_alpha_);
}

Point UiGraphBuffer::GetVisualCenter() const {
  const auto center = buffer_.GetVisualCenter(Convert(HitTestBounds()),
      Convert(HitTestBounds()), x_mode_, y_mode_, half_alpha_);
  return {center.x, center.y};
}

}  // namespace srhd_awa::platform::ui

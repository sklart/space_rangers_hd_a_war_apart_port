#include "ui_gai.hpp"

#include "scene_compositor.hpp"
#include "software_compositor.hpp"

#include <algorithm>
#include <memory>

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* reason) { if (error) *error = reason; return false; }
}

UiGaiLeaf* UiObject::AddGai() {
  auto child = std::make_unique<UiGaiLeaf>();
  auto* result = child.get();
  Attach(std::move(child));
  return result;
}

bool UiGaiLeaf::LoadResource(const std::string& resource, std::string* error) {
  if (!animation_.LoadResource(resource, error)) return false;
  const auto& meta = animation_.Metadata();
  if (ClientSize().width <= 0 || ClientSize().height <= 0)
    SetSize({meta.right - meta.left, meta.bottom - meta.top});
  return true;
}

bool UiGaiLeaf::LoadBytes(const std::uint8_t* bytes, std::size_t size,
                          const std::string& resource, std::string* error) {
  if (!animation_.LoadBytes(bytes, size, resource, error)) return false;
  const auto& meta = animation_.Metadata();
  if (ClientSize().width <= 0 || ClientSize().height <= 0)
    SetSize({meta.right - meta.left, meta.bottom - meta.top});
  return true;
}

bool UiGaiLeaf::Update(std::uint64_t delta_ms, std::string* error) {
  if (!animation_.Update(delta_ms, error)) return false;
  if (stop_after_one_cycle_ && !animation_.Running()) SetActive(false);
  return true;
}

bool UiGaiLeaf::Render(const scene_compositor::Framebuffer& target, Rect clip,
                       std::string* error) const {
  if (!animation_.IsLoaded()) return Fail(error, "UI GAI is not loaded");
  const auto& meta = animation_.Metadata();
  const auto& frame = animation_.Image();
  const auto bounds = HitTestBounds();
  const auto plan = image_layout::MakePlan({bounds.left, bounds.top, bounds.right, bounds.bottom},
                                           {clip.left, clip.top, clip.right, clip.bottom},
                                           meta.right - meta.left, meta.bottom - meta.top,
                                           x_mode_, y_mode_);
  const software_compositor::Rect compositor_clip{clip.left, clip.top, clip.right, clip.bottom};
  for (const auto& tile : plan.tiles)
    if (!software_compositor::CompositeBGRA(target.pixels, target.width, target.height,
                                            target.pitch_pixels, frame.pixels.data(), frame.width,
                                            frame.height, frame.pitch,
                                            tile.x + animation_.FrameOffsetX(),
                                            tile.y + animation_.FrameOffsetY(),
                                            software_compositor::BlendMode::Alpha,
                                            &compositor_clip, error, alpha_)) return false;
  return true;
}

bool UiGaiLeaf::HitTestPixel(Point point) const {
  if (!Active() || !animation_.IsLoaded() || !alpha_ || !Contains(HitTestBounds(), point))
    return false;
  const auto& meta = animation_.Metadata();
  const auto& frame = animation_.Image();
  const auto bounds = HitTestBounds();
  const auto plan = image_layout::MakePlan({bounds.left, bounds.top, bounds.right, bounds.bottom},
                                           {point.x, point.y, point.x + 1, point.y + 1},
                                           meta.right - meta.left, meta.bottom - meta.top,
                                           x_mode_, y_mode_);
  std::uint16_t rendered{};
  const software_compositor::Rect one{0, 0, 1, 1};
  for (const auto& tile : plan.tiles) {
    if (!software_compositor::CompositeBGRA(&rendered, 1, 1, 1, frame.pixels.data(),
                                            frame.width, frame.height, frame.pitch,
                                            tile.x + animation_.FrameOffsetX() - point.x,
                                            tile.y + animation_.FrameOffsetY() - point.y,
                                            software_compositor::BlendMode::Alpha,
                                            &one, nullptr, alpha_)) return false;
  }
  return rendered != 0;
}

}  // namespace srhd_awa::platform::ui

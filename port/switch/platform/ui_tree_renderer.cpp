#include "ui_tree_renderer.hpp"

namespace srhd_awa::platform::ui {
namespace {
bool Fail(std::string* error, const char* message) { if (error) *error = message; return false; }
okgf_rle_bridge::Rect ToImageRect(Rect value) { return {value.left, value.top, value.right, value.bottom}; }
}  // namespace

UiImageLeaf* UiObject::AddImage() {
  auto child = std::make_unique<UiImageLeaf>(); auto* result = child.get(); Attach(std::move(child)); return result;
}
UiGILeaf* UiObject::AddGIObject() {
  auto child = std::make_unique<UiGILeaf>(); auto* result = child.get(); Attach(std::move(child)); return result;
}

bool UiImageLeaf::Load(image_object::Kind kind, const std::string& resource, const std::string& option, std::string* error) {
  if (!image_.Load(kind, resource, option, error)) return false;
  if (ClientSize().width <= 0 || ClientSize().height <= 0)
    SetSize({image_.natural_width(), image_.natural_height()});
  return true;
}
bool UiImageLeaf::Render(const scene_compositor::Framebuffer& target, Rect clip, std::string* error) const {
  const Rect bounds = HitTestBounds();
  return image_.DrawFramebufferAt(target.pixels, target.width, target.height, target.pitch_pixels,
                                  bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                                  ToImageRect(clip), error);
}
bool UiGILeaf::LoadResource(const std::string& resource, std::string* error) {
  if (!image_.LoadResource(resource, error)) return false;
  if (ClientSize().width <= 0 || ClientSize().height <= 0)
    SetSize({image_.Image().width, image_.Image().height});
  return true;
}
bool UiGILeaf::Update(std::uint64_t delta_ms, std::string* error) { return image_.Update(delta_ms, error); }
bool UiGILeaf::Render(const scene_compositor::Framebuffer& target, Rect clip, std::string* error) const {
  const Rect bounds = HitTestBounds();
  return image_.DrawFramebufferAt(target.pixels, target.width, target.height, target.pitch_pixels, bounds.left, bounds.top,
                                  clip.left, clip.top, clip.right, clip.bottom, error);
}

bool UiTreeRenderer::Update(UiObject& root, std::uint64_t delta_ms, std::string* error) {
  return UpdateNode(root, delta_ms, error);
}
bool UiTreeRenderer::UpdateNode(UiObject& node, std::uint64_t delta_ms, std::string* error) {
  if (!node.Active()) return true;
  if (auto* gi = dynamic_cast<UiGILeaf*>(&node); gi && !gi->Update(delta_ms, error)) return false;
  for (const auto& child : node.Children()) if (!UpdateNode(*child, delta_ms, error)) return false;
  return true;
}
bool UiTreeRenderer::Render(const UiObject& root, const scene_compositor::Framebuffer& target, std::string* error) {
  if (!target.pixels || target.width <= 0 || target.height <= 0 || target.pitch_pixels < target.width)
    return Fail(error, "UI renderer framebuffer is invalid");
  return RenderNode(root, target, {0, 0, target.width, target.height}, error);
}
bool UiTreeRenderer::RenderNode(const UiObject& node, const scene_compositor::Framebuffer& target, Rect clip, std::string* error) {
  if (!node.Active()) return true;
  if (auto* image = dynamic_cast<const UiImageLeaf*>(&node)) return image->Render(target, clip, error);
  if (auto* gi = dynamic_cast<const UiGILeaf*>(&node)) return gi->Render(target, clip, error);
  for (const auto& child : node.Children()) {
    const Rect child_clip = Intersect(clip, child->HitTestBounds());
    if (!IsEmpty(child_clip) && !RenderNode(*child, target, child_clip, error)) return false;
  }
  return true;
}

bool UiTree::Update(std::uint64_t delta_ms, std::string* error) { return UiTreeRenderer::Update(*root_, delta_ms, error); }
bool UiTree::Render(const scene_compositor::Framebuffer& target, std::string* error) const { return UiTreeRenderer::Render(*root_, target, error); }

}  // namespace srhd_awa::platform::ui

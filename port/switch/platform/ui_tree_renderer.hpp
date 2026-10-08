#pragma once

#include "gi_object.hpp"
#include "image_object.hpp"
#include "scene_compositor.hpp"
#include "ui_object.hpp"

#include <cstdint>
#include <cstddef>
#include <string>

namespace srhd_awa::package { class Package; }
namespace srhd_awa::platform::ui {

class UiImageLeaf final : public UiObject {
 public:
  explicit UiImageLeaf(package::Package* package = nullptr) : image_(package) {}
  NodeKind Kind() const override { return NodeKind::ImageLeaf; }
  image_object::PortableImageObject& Image() { return image_; }
  const image_object::PortableImageObject& Image() const { return image_; }
  bool Load(image_object::Kind kind, const std::string& resource, const std::string& option,
            std::string* error = nullptr);
  bool LoadBytes(image_object::Kind kind, const std::uint8_t* bytes, std::size_t size,
                 const std::string& resource, const std::string& option,
                 std::string* error = nullptr);
  bool Render(const scene_compositor::Framebuffer& target, Rect clip, std::string* error = nullptr) const;

 private:
  image_object::PortableImageObject image_;
};

class UiGILeaf final : public UiObject {
 public:
  explicit UiGILeaf(package::Package* package = nullptr) : image_(package) {}
  NodeKind Kind() const override { return NodeKind::GILeaf; }
  gi_object::GIObject& Image() { return image_; }
  const gi_object::GIObject& Image() const { return image_; }
  bool LoadResource(const std::string& resource, std::string* error = nullptr);
  bool LoadBytes(const std::uint8_t* bytes, std::size_t size,
                 const std::string& resource, std::string* error = nullptr);
  bool Update(std::uint64_t delta_ms, std::string* error = nullptr);
  bool Render(const scene_compositor::Framebuffer& target, Rect clip, std::string* error = nullptr) const;

 private:
  gi_object::GIObject image_;
};

class UiTreeRenderer {
 public:
  static bool Update(UiObject& root, std::uint64_t delta_ms, std::string* error = nullptr);
  static bool Render(const UiObject& root, const scene_compositor::Framebuffer& target, std::string* error = nullptr);

 private:
  static bool UpdateNode(UiObject& node, std::uint64_t delta_ms, std::string* error);
  static bool RenderNode(const UiObject& node, const scene_compositor::Framebuffer& target, Rect clip, std::string* error);
};

}  // namespace srhd_awa::platform::ui

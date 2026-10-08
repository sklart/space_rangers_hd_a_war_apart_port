#pragma once

#include "scene_compositor.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::gi_object { class GIObject; }
namespace srhd_awa::platform::image_object { class PortableImageObject; }

namespace srhd_awa::platform::presentation_scene {

struct FramebufferFingerprint { std::uint32_t crc32{}; std::uint64_t fnv64{}; std::size_t bytes{}; };

// A minimal mixed M20/M21 scene. Entries are borrowed and never outlive their
// owners; this is deliberately not a UI tree, cache, or renderer owner.
class PresentationScene {
 public:
  bool AddGI(const std::string& id, const gi_object::GIObject* object, std::string* error = nullptr);
  bool AddImage(const std::string& id, const image_object::PortableImageObject* object, std::string* error = nullptr);
  void Clear();
  bool Render(const scene_compositor::Framebuffer& target, std::string* error = nullptr) const;
  bool ComputeFramebufferFingerprint(const scene_compositor::Framebuffer& target,
                                     FramebufferFingerprint* result, std::string* error = nullptr) const;
  std::size_t EntryCount() const { return entries_.size(); }

 private:
  struct Entry { std::string id; const gi_object::GIObject* gi{}; const image_object::PortableImageObject* image{}; };
  std::vector<Entry> entries_;
};

}  // namespace srhd_awa::platform::presentation_scene

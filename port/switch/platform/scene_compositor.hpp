#pragma once

#include "gi_format0_cpu.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::scene_compositor {

struct Framebuffer {
  std::uint16_t* pixels{};
  std::int32_t width{};
  std::int32_t height{};
  std::int32_t pitch_pixels{};
};

struct SceneSprite {
  std::string id;
  gi_format0_cpu::CpuImage image;
  std::int32_t x{};
  std::int32_t y{};
  std::uint8_t alpha{255};
  std::int32_t layer{};
  bool visible{true};
};

struct Fingerprint {
  std::uint32_t crc32{};
  std::uint64_t fnv64{};
  std::size_t canonical_bytes{};
};

class Scene {
 public:
  bool AddSprite(SceneSprite sprite, std::string* error = nullptr);
  bool RemoveSprite(const std::string& id);
  void Clear();
  bool Render(const Framebuffer& target, std::string* error = nullptr) const;
  bool ComputeFingerprint(Fingerprint* result, std::string* error = nullptr) const;
  std::size_t SpriteCount() const { return sprites_.size(); }
  const std::vector<SceneSprite>& Sprites() const { return sprites_; }

 private:
  std::vector<SceneSprite> sprites_;
};

}  // namespace srhd_awa::platform::scene_compositor

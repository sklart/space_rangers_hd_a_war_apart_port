#include "scene_compositor.hpp"

#include "software_compositor.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::scene_compositor {
namespace {

bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}

bool ValidImage(const gi_format0_cpu::CpuImage& image) {
  return image.width > 0 && image.height > 0 && image.bytes_per_pixel == 4 &&
         static_cast<std::int64_t>(image.pitch) >= static_cast<std::int64_t>(image.width) * 4 &&
         static_cast<std::uint64_t>(image.pitch) * image.height == image.pixels.size();
}

void AppendU32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes->push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t Crc32(const std::uint8_t* bytes, std::size_t size) {
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t index = 0; index < size; ++index) {
    crc ^= bytes[index];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

std::uint64_t Fnv64(const std::uint8_t* bytes, std::size_t size) {
  std::uint64_t value = UINT64_C(14695981039346656037);
  for (std::size_t index = 0; index < size; ++index)
    value = (value ^ bytes[index]) * UINT64_C(1099511628211);
  return value;
}

std::vector<const SceneSprite*> Ordered(const std::vector<SceneSprite>& sprites) {
  std::vector<const SceneSprite*> ordered;
  ordered.reserve(sprites.size());
  for (const auto& sprite : sprites) ordered.push_back(&sprite);
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
    return left->layer < right->layer;
  });
  return ordered;
}

}  // namespace

bool Scene::AddSprite(SceneSprite sprite, std::string* error) {
  if (sprite.id.empty()) return Fail(error, "scene sprite id is empty");
  if (!ValidImage(sprite.image)) return Fail(error, "scene sprite image is invalid");
  if (std::any_of(sprites_.begin(), sprites_.end(), [&sprite](const auto& item) { return item.id == sprite.id; }))
    return Fail(error, "scene sprite id is duplicated");
  sprites_.push_back(std::move(sprite));
  return true;
}

bool Scene::RemoveSprite(const std::string& id) {
  const auto previous_size = sprites_.size();
  sprites_.erase(std::remove_if(sprites_.begin(), sprites_.end(), [&id](const auto& item) { return item.id == id; }), sprites_.end());
  return sprites_.size() != previous_size;
}

void Scene::Clear() { sprites_.clear(); }

bool Scene::Render(const Framebuffer& target, std::string* error) const {
  if (!target.pixels || target.width <= 0 || target.height <= 0 || target.pitch_pixels < target.width)
    return Fail(error, "scene framebuffer is invalid");
  for (const auto* sprite : Ordered(sprites_)) {
    if (!sprite->visible) continue;
    if (sprite->alpha == 255) {
      if (!software_compositor::CompositeBGRA(target.pixels, target.width, target.height, target.pitch_pixels,
                                               sprite->image.pixels.data(), sprite->image.width, sprite->image.height,
                                               sprite->image.pitch, sprite->x, sprite->y,
                                               software_compositor::BlendMode::Opaque, nullptr, error)) return false;
      continue;
    }
    std::vector<std::uint8_t> alpha_pixels(sprite->image.pixels);
    for (std::size_t index = 3; index < alpha_pixels.size(); index += 4)
      alpha_pixels[index] = static_cast<std::uint8_t>((static_cast<std::uint16_t>(alpha_pixels[index]) * sprite->alpha + 127u) / 255u);
    if (!software_compositor::CompositeBGRA(target.pixels, target.width, target.height, target.pitch_pixels,
                                             alpha_pixels.data(), sprite->image.width, sprite->image.height,
                                             sprite->image.pitch, sprite->x, sprite->y,
                                             software_compositor::BlendMode::Alpha, nullptr, error)) return false;
  }
  return true;
}

bool Scene::ComputeFingerprint(Fingerprint* result, std::string* error) const {
  if (!result) return Fail(error, "scene fingerprint output is null");
  std::vector<std::uint8_t> canonical;
  for (const auto* sprite : Ordered(sprites_)) {
    if (!ValidImage(sprite->image) || sprite->id.size() > std::numeric_limits<std::uint32_t>::max())
      return Fail(error, "scene fingerprint sprite is invalid");
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->id.size()));
    canonical.insert(canonical.end(), sprite->id.begin(), sprite->id.end());
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->layer));
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->x));
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->y));
    canonical.push_back(sprite->alpha);
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->image.width));
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->image.height));
    AppendU32(&canonical, static_cast<std::uint32_t>(sprite->image.pitch));
    canonical.insert(canonical.end(), sprite->image.pixels.begin(), sprite->image.pixels.end());
  }
  result->canonical_bytes = canonical.size();
  result->crc32 = Crc32(canonical.data(), canonical.size());
  result->fnv64 = Fnv64(canonical.data(), canonical.size());
  return true;
}

}  // namespace srhd_awa::platform::scene_compositor

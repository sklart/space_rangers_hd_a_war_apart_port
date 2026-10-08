#pragma once

#include "gai_cpu.hpp"
#include "gai_playback_cpu.hpp"
#include "gi_format0_cpu.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace srhd_awa::package { class Package; }
namespace srhd_awa::platform::scene_compositor { class Scene; }

namespace srhd_awa::platform::gi_object {

class GIObject {
 public:
  explicit GIObject(package::Package* package = nullptr) : package_(package) {}

  void SetPackage(package::Package* package) { package_ = package; }
  void SetId(std::string id) { id_ = std::move(id); }
  bool LoadResource(const std::string& resource, std::string* error = nullptr);
  bool LoadBytes(const std::uint8_t* bytes, std::size_t size,
                 const std::string& resource, std::string* error = nullptr);
  void SetPosition(std::int32_t x, std::int32_t y) { x_ = x; y_ = y; }
  void SetLayer(std::int32_t layer) { layer_ = layer; }
  void SetAlpha(std::uint8_t alpha) { alpha_ = alpha; }
  void SetVisible(bool visible) { visible_ = visible; }
  bool Update(std::uint64_t delta_ms, std::string* error = nullptr);
  bool Draw(scene_compositor::Scene& scene, std::string* error = nullptr) const;
  bool DrawFramebufferAt(std::uint16_t* pixels, std::int32_t width, std::int32_t height, std::int32_t pitch,
                         std::int32_t x, std::int32_t y, std::int32_t clip_left, std::int32_t clip_top,
                         std::int32_t clip_right, std::int32_t clip_bottom, std::string* error = nullptr) const;

  bool IsLoaded() const { return loaded_; }
  const std::string& Id() const { return id_; }
  const std::string& Resource() const { return resource_; }
  const gai_cpu::GaiMetadata& Metadata() const { return metadata_; }
  std::int32_t SequenceFrame() const { return playback_.sequence_frame; }
  std::int32_t SourceFrame() const { return source_frame_; }
  const gi_format0_cpu::CpuImage& Image() const { return image_; }
  std::int32_t X() const { return x_; }
  std::int32_t Y() const { return y_; }
  std::int32_t Layer() const { return layer_; }
  std::uint8_t Alpha() const { return alpha_; }
  bool Visible() const { return visible_; }

 private:
  bool LoadDecoded(std::vector<std::uint8_t> bytes, const std::string& resource,
                   std::string* error);
  bool DecodeCurrentFrame(std::string* error);

  package::Package* package_{};
  std::string id_;
  std::string resource_;
  std::vector<std::uint8_t> gai_bytes_;
  gai_cpu::GaiMetadata metadata_{};
  gai_cpu::GaiSequence sequence_{};
  gai_playback_cpu::State playback_{};
  std::int32_t source_frame_{};
  gi_format0_cpu::CpuImage image_;
  std::int32_t x_{}, y_{}, layer_{};
  std::uint8_t alpha_{255};
  bool visible_{true};
  bool loaded_{};
};

}  // namespace srhd_awa::platform::gi_object

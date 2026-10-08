#include "gi_object.hpp"

#include "gi_format2_cpu.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"
#include "software_compositor.hpp"

namespace srhd_awa::platform::gi_object {
namespace {

bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}

}  // namespace

bool GIObject::DecodeCurrentFrame(std::string* error) {
  if (source_frame_ < 0 || source_frame_ >= metadata_.frame_count)
    return Fail(error, "GI object source frame is invalid");
  gai_cpu::GaiFramePayload payload;
  if (gai_cpu::ExtractGaiFrame(gai_bytes_.data(), gai_bytes_.size(), source_frame_, &payload, error) != gai_cpu::Status::Ok)
    return false;
  gi_format0_cpu::Metadata format0_metadata{};
  gi_format0_cpu::CpuImage decoded;
  std::string format0_error;
  if (gi_format0_cpu::Decode(payload.gi_bytes.data(), payload.gi_bytes.size(), &format0_metadata, &decoded,
                              &format0_error) != gi_format0_cpu::Status::Ok) {
    gi_format2_cpu::Metadata format2_metadata{};
    if (gi_format2_cpu::Decode(payload.gi_bytes.data(), payload.gi_bytes.size(), &format2_metadata, &decoded,
                               error) != gi_format2_cpu::Status::Ok)
      return false;
  }
  image_ = std::move(decoded);
  return true;
}

bool GIObject::LoadResource(const std::string& resource, std::string* error) {
  if (error) error->clear();
  if (!package_) return Fail(error, "GI object package is null");
  const auto* entry = package_->Resolve(resource);
  std::vector<std::uint8_t> bytes;
  gai_cpu::GaiMetadata metadata{};
  if (!entry || !package_->ReadPayload(*entry, &bytes, error) ||
      gai_cpu::ValidateGai(bytes.data(), bytes.size(), &metadata, error) != gai_cpu::Status::Ok)
    return false;
  gai_cpu::GaiSequence sequence;
  if (metadata.sequence_table_present && metadata.sequence_count > 0) {
    if (gai_cpu::ReadGaiSequence(bytes.data(), bytes.size(), 0, &sequence, error) != gai_cpu::Status::Ok)
      return false;
  } else {
    sequence.index = 0;
    sequence.frames.push_back({0, 1});
  }
  gai_playback_cpu::State playback;
  if (!gai_playback_cpu::Initialize(&playback, sequence, error)) return false;
  gai_bytes_ = std::move(bytes);
  metadata_ = metadata;
  sequence_ = std::move(sequence);
  playback_ = playback;
  resource_ = resource;
  if (id_.empty()) id_ = "gi-object:" + resource;
  source_frame_ = sequence_.frames.front().source_frame_index;
  loaded_ = false;
  if (!DecodeCurrentFrame(error)) return false;
  loaded_ = true;
  return true;
}

bool GIObject::Update(std::uint64_t delta_ms, std::string* error) {
  if (error) error->clear();
  if (!loaded_) return Fail(error, "GI object is not loaded");
  std::vector<gai_playback_cpu::Step> entered;
  if (!gai_playback_cpu::AdvanceBy(&playback_, sequence_, delta_ms, &entered, error)) return false;
  if (entered.empty()) return true;
  const auto next_source = sequence_.frames[playback_.sequence_frame].source_frame_index;
  if (next_source == source_frame_) return true;
  source_frame_ = next_source;
  return DecodeCurrentFrame(error);
}

bool GIObject::Draw(scene_compositor::Scene& scene, std::string* error) const {
  if (error) error->clear();
  if (!loaded_) return Fail(error, "GI object is not loaded");
  if (id_.empty()) return Fail(error, "GI object id is empty");
  scene.RemoveSprite(id_);
  scene_compositor::SceneSprite sprite{id_, image_, x_, y_, alpha_, layer_, visible_};
  return scene.AddSprite(std::move(sprite), error);
}

bool GIObject::DrawFramebufferAt(std::uint16_t* pixels, std::int32_t width, std::int32_t height, std::int32_t pitch,
                                 std::int32_t x, std::int32_t y, std::int32_t clip_left, std::int32_t clip_top,
                                 std::int32_t clip_right, std::int32_t clip_bottom, std::string* error) const {
  if (!loaded_) return Fail(error, "GI object is not loaded");
  if (!visible_) return true;
  if (!pixels || width <= 0 || height <= 0 || pitch < width) return Fail(error, "GI object framebuffer is invalid");
  const software_compositor::Rect clip{clip_left, clip_top, clip_right, clip_bottom};
  const auto mode = alpha_ == 255 ? software_compositor::BlendMode::Opaque : software_compositor::BlendMode::Alpha;
  if (mode == software_compositor::BlendMode::Opaque)
    return software_compositor::CompositeBGRA(pixels, width, height, pitch, image_.pixels.data(), image_.width,
                                               image_.height, image_.pitch, x, y, mode, &clip, error);
  std::vector<std::uint8_t> alpha_pixels = image_.pixels;
  for (std::size_t index = 3; index < alpha_pixels.size(); index += 4)
    alpha_pixels[index] = static_cast<std::uint8_t>((static_cast<std::uint16_t>(alpha_pixels[index]) * alpha_ + 127u) / 255u);
  return software_compositor::CompositeBGRA(pixels, width, height, pitch, alpha_pixels.data(), image_.width,
                                             image_.height, image_.pitch, x, y, mode, &clip, error);
}

}  // namespace srhd_awa::platform::gi_object

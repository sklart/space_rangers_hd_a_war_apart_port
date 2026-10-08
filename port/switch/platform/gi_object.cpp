#include "gi_object.hpp"

#include "gi_format2_cpu.hpp"
#include "gai_frame_sequence_cpu.hpp"
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
    frame_left_ = format2_metadata.left; frame_top_ = format2_metadata.top;
  } else {
    frame_left_ = format0_metadata.left; frame_top_ = format0_metadata.top;
  }
  image_ = std::move(decoded);
  return true;
}

bool GIObject::LoadResource(const std::string& resource, std::string* error) {
  if (error) error->clear();
  if (!package_) return Fail(error, "GI object package is null");
  const auto* entry = package_->Resolve(resource);
  std::vector<std::uint8_t> bytes;
  if (entry && entry->data_size > (256u << 20)) return Fail(error, "GI object source exceeds limit");
  if (!entry || !package_->ReadPayload(*entry, &bytes, error)) return false;
  return LoadDecoded(std::move(bytes), resource, error);
}

bool GIObject::LoadBytes(const std::uint8_t* source, std::size_t size,
                         const std::string& resource, std::string* error) {
  if (error) error->clear();
  if (!source || size == 0 || size > (256u << 20)) return Fail(error, "GI object source bytes are invalid");
  return LoadDecoded(std::vector<std::uint8_t>(source, source + size), resource, error);
}

bool GIObject::LoadDecoded(std::vector<std::uint8_t> bytes,
                           const std::string& resource, std::string* error) {
  gai_cpu::GaiMetadata metadata{};
  if (gai_cpu::ValidateGai(bytes.data(), bytes.size(), &metadata, error) != gai_cpu::Status::Ok)
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
  const auto previous_playback = playback_;
  const auto previous_source = source_frame_;
  std::vector<gai_playback_cpu::Step> entered;
  if (!gai_playback_cpu::AdvanceBy(&playback_, sequence_, delta_ms, &entered, error)) {
    playback_ = previous_playback;
    return false;
  }
  if (entered.empty()) return true;
  const auto next_source = sequence_.frames[playback_.sequence_frame].source_frame_index;
  if (next_source == source_frame_) return true;
  source_frame_ = next_source;
  if (DecodeCurrentFrame(error)) return true;
  source_frame_ = previous_source;
  playback_ = previous_playback;
  DecodeCurrentFrame(nullptr);
  return false;
}

bool GIObject::SelectSequence(gai_cpu::GaiSequence sequence, std::string* error) {
  if (!loaded_ || sequence.frames.empty()) return Fail(error, "GI object sequence is empty");
  for (const auto& frame : sequence.frames)
    if (frame.source_frame_index < 0 || frame.source_frame_index >= metadata_.frame_count ||
        frame.delay_ms <= 0) return Fail(error, "GI object sequence frame is invalid");
  gai_playback_cpu::State next{};
  if (!gai_playback_cpu::Initialize(&next, sequence, error)) return false;
  const auto previous_source = source_frame_;
  source_frame_ = sequence.frames.front().source_frame_index;
  if (source_frame_ != previous_source && !DecodeCurrentFrame(error)) {
    source_frame_ = previous_source;
    DecodeCurrentFrame(nullptr);
    return false;
  }
  sequence_ = std::move(sequence); playback_ = next;
  return true;
}

bool GIObject::SelectEmbeddedSequence(std::int32_t index, std::string* error) {
  if (!loaded_) return Fail(error, "GI object is not loaded");
  gai_cpu::GaiSequence sequence{};
  if (gai_cpu::ReadGaiSequence(gai_bytes_.data(), gai_bytes_.size(), index,
                                &sequence, error) != gai_cpu::Status::Ok) return false;
  return SelectSequence(std::move(sequence), error);
}

bool GIObject::SelectCustomSequence(const std::string& text, std::string* error) {
  if (!loaded_) return Fail(error, "GI object is not loaded");
  gai_cpu::GaiSequence sequence{};
  if (!gai_frame_sequence_cpu::Parse(text, metadata_.frame_count, &sequence, error)) return false;
  return SelectSequence(std::move(sequence), error);
}

bool GIObject::SetFramePosition(std::int32_t frame, bool forward_only, std::string* error) {
  const auto previous_playback = playback_;
  const auto previous_source = source_frame_;
  if (!loaded_ || !gai_playback_cpu::SetFramePosition(&playback_, sequence_, frame,
                                                       forward_only, error)) return false;
  const auto source = sequence_.frames[playback_.sequence_frame].source_frame_index;
  if (source == source_frame_) return true;
  source_frame_ = source;
  if (DecodeCurrentFrame(error)) return true;
  source_frame_ = previous_source;
  playback_ = previous_playback;
  DecodeCurrentFrame(nullptr);
  return false;
}

bool GIObject::Stop(std::string* error) {
  return gai_playback_cpu::Stop(&playback_, error);
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
  return software_compositor::CompositeBGRA(pixels, width, height, pitch, image_.pixels.data(), image_.width,
                                             image_.height, image_.pitch, x, y, mode, &clip, error, alpha_);
}

}  // namespace srhd_awa::platform::gi_object

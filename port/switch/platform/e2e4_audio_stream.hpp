#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace srhd_awa::platform::e2e4_audio {

// Three fixed-size PCM chunks. All methods are serialized with the SDL audio
// device lock. Render is called only by the callback and never allocates,
// decodes, performs I/O, or signals an OS event.
class StreamRing {
public:
    static constexpr std::size_t kChunks = 3;
    static constexpr std::size_t kMaxBytes = 2 * 1024 * 1024;

    bool Initialize(std::size_t chunk_bytes, std::uint32_t source_rate,
                    std::uint32_t output_rate, std::uint8_t channels,
                    std::uint8_t bits) {
        const std::size_t align = static_cast<std::size_t>(channels) * (bits / 8);
        if (!source_rate || source_rate > 192000 || !output_rate ||
            output_rate > 192000 || (channels != 1 && channels != 2) ||
            (bits != 8 && bits != 16) || !align || !chunk_bytes ||
            chunk_bytes % align || chunk_bytes > kMaxBytes / kChunks) return false;
        bytes_.assign(chunk_bytes * kChunks, bits == 8 ? 128 : 0);
        chunk_bytes_ = chunk_bytes;
        channels_ = channels;
        bits_ = bits;
        frame_bytes_ = align;
        source_rate_ = source_rate;
        output_rate_ = output_rate;
        position_ = 0.0;
        played_frames_ = 0;
        completed_.fill(0);
        ready_.fill(false);
        playing_ = false;
        return true;
    }

    // A chunk becomes audible only after a full replacement. A short final
    // decode is padded with PCM silence, never with stale previous music.
    bool WriteChunk(std::size_t index, const void* data, std::size_t bytes) {
        if (index >= kChunks || chunk_bytes_ == 0 || bytes > chunk_bytes_ ||
            bytes % frame_bytes_ || (bytes && !data)) return false;
        if (playing_ && index == CurrentChunk()) return false;
        auto* dest = bytes_.data() + index * chunk_bytes_;
        if (bits_ == 8) std::fill(dest, dest + chunk_bytes_, 128);
        else std::fill(dest, dest + chunk_bytes_, 0);
        if (bytes) std::copy_n(static_cast<const std::uint8_t*>(data), bytes, dest);
        ready_[index] = true;
        return true;
    }

    bool Start() {
        if (!ready_[0] || !ready_[1] || !ready_[2]) return false;
        playing_ = true;
        position_ = 0.0;
        played_frames_ = 0;
        completed_.fill(0);
        return true;
    }

    void Stop() { playing_ = false; }
    bool IsPlaying() const { return playing_; }
    std::uint64_t PlayedFrames() const { return played_frames_; }
    std::uint32_t PlayCursorBytes() const {
        return static_cast<std::uint32_t>(static_cast<std::size_t>(position_) * frame_bytes_);
    }
    std::size_t CurrentChunk() const {
        return chunk_bytes_ ?
            (static_cast<std::size_t>(position_) * frame_bytes_ / chunk_bytes_) % kChunks : 0;
    }

    // Event delivery belongs to the non-callback thread. Counts are retained
    // if that thread misses more than one revolution of the ring.
    std::array<std::uint32_t, kChunks> DrainCompleted() {
        auto result = completed_;
        completed_.fill(0);
        return result;
    }

    // Adds interleaved stereo samples to an existing S16 mix. The call is
    // bounded by the requested frame count and the preallocated ring size.
    void RenderAdd(std::int16_t* output, std::size_t frames, float gain = 1.0f) {
        if (!output || !playing_ || !std::isfinite(gain)) return;
        gain = std::clamp(gain, 0.0f, 1.0f);
        const std::size_t total_frames = bytes_.size() / frame_bytes_;
        const std::size_t chunk_frames = chunk_bytes_ / frame_bytes_;
        const double step = static_cast<double>(source_rate_) / output_rate_;
        for (std::size_t i = 0; i < frames; ++i) {
            const std::size_t frame = static_cast<std::size_t>(position_);
            const std::size_t chunk = frame / chunk_frames;
            if (ready_[chunk]) {
                const std::size_t next = (frame + 1) % total_frames;
                const float fraction = static_cast<float>(position_ - frame);
                for (std::size_t channel = 0; channel < 2; ++channel) {
                    const std::size_t input_channel = channels_ == 1 ? 0 : channel;
                    const float first = Sample(frame, input_channel);
                    const float second = ready_[next / chunk_frames] ?
                        Sample(next, input_channel) : first;
                    const float mixed = static_cast<float>(output[i * 2 + channel]) / 32768.0f +
                        (first + (second - first) * fraction) * gain;
                    const float limited = std::clamp(mixed, -1.0f, 1.0f);
                    output[i * 2 + channel] = static_cast<std::int16_t>(
                        std::lround(limited < 0 ? limited * 32768.0f :
                                    limited * 32767.0f));
                }
            }
            const double advanced = position_ + step;
            const std::size_t old_boundary = frame / chunk_frames;
            const std::size_t crossed = static_cast<std::size_t>(advanced / chunk_frames);
            if (crossed > old_boundary) {
                for (std::size_t boundary = old_boundary; boundary < crossed; ++boundary) {
                    const std::size_t done = boundary % kChunks;
                    ready_[done] = false;
                    if (completed_[done] != std::numeric_limits<std::uint32_t>::max())
                        ++completed_[done];
                }
            }
            position_ = std::fmod(advanced, static_cast<double>(total_frames));
            ++played_frames_;
        }
    }

private:
    float Sample(std::size_t frame, std::size_t channel) const {
        const std::size_t offset = (frame * channels_ + channel) * (bits_ / 8);
        if (bits_ == 8) return (static_cast<int>(bytes_[offset]) - 128) / 128.0f;
        const auto value = static_cast<std::uint16_t>(bytes_[offset]) |
            static_cast<std::uint16_t>(bytes_[offset + 1]) << 8;
        return static_cast<std::int16_t>(value) / 32768.0f;
    }

    std::vector<std::uint8_t> bytes_;
    std::array<std::uint32_t, kChunks> completed_{};
    std::array<bool, kChunks> ready_{};
    std::size_t chunk_bytes_ = 0;
    std::size_t frame_bytes_ = 0;
    std::uint32_t source_rate_ = 0;
    std::uint32_t output_rate_ = 0;
    std::uint64_t played_frames_ = 0;
    std::uint8_t channels_ = 0;
    std::uint8_t bits_ = 0;
    double position_ = 0.0;
    bool playing_ = false;
};

} // namespace srhd_awa::platform::e2e4_audio

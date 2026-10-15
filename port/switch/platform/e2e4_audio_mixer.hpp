#pragma once

#include <algorithm>
#include <array>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace srhd_awa::platform::e2e4_audio {

enum class Bus : std::uint8_t { effects, music };

struct Pcm {
    std::vector<std::uint8_t> bytes;
    std::uint32_t rate = 0;
    std::uint8_t channels = 0;
    std::uint8_t bits = 0;

    std::size_t Frames() const {
        const std::size_t align = static_cast<std::size_t>(channels) * (bits / 8);
        return align ? bytes.size() / align : 0;
    }
    bool Valid() const {
        const std::size_t align = static_cast<std::size_t>(channels) * (bits / 8);
        return rate > 0 && rate <= 192000 && (channels == 1 || channels == 2) &&
               (bits == 8 || bits == 16) && align != 0 && !bytes.empty() &&
               bytes.size() % align == 0;
    }
};

class Mixer {
public:
    static constexpr std::size_t kMaxVoices = 64;
    explicit Mixer(std::uint32_t output_rate) : output_rate_(output_rate) {}

    // The caller serializes Add/Stop/Set/Collect with Mix, e.g. with
    // SDL_LockAudioDevice. Mix itself performs no allocation or file I/O.
    std::uint64_t Add(std::shared_ptr<const Pcm> pcm, Bus bus, bool loop,
                      float gain = 1.0f, float pan = 0.0f) {
        if (output_rate_ == 0 || !pcm || !pcm->Valid()) return 0;
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            Slot& slot = slots_[i];
            if (slot.pcm) continue; // Retired voices are reclaimed on caller thread.
            ++slot.generation;
            if (slot.generation == 0) ++slot.generation;
            slot.pcm = std::move(pcm);
            slot.bus = bus;
            slot.loop = loop;
            slot.active = true;
            slot.position = 0.0;
            slot.gain = FiniteUnit(gain);
            slot.target_gain = slot.gain;
            slot.fade_step = 0.0f;
            slot.fade_frames = 0;
            slot.pan = FinitePan(pan);
            return (static_cast<std::uint64_t>(slot.generation) << 32) | (i + 1);
        }
        return 0;
    }

    bool Stop(std::uint64_t handle) {
        Slot* slot = Find(handle);
        if (!slot) return false;
        slot->active = false;
        return true;
    }

    bool SetGain(std::uint64_t handle, float value, std::uint32_t fade_frames = 0) {
        Slot* slot = Find(handle);
        if (!slot) return false;
        slot->target_gain = FiniteUnit(value);
        slot->fade_frames = fade_frames;
        slot->fade_step = fade_frames ?
            (slot->target_gain - slot->gain) / static_cast<float>(fade_frames) : 0.0f;
        if (!fade_frames) slot->gain = slot->target_gain;
        return true;
    }

    bool SetPan(std::uint64_t handle, float value) {
        Slot* slot = Find(handle);
        if (!slot) return false;
        slot->pan = FinitePan(value);
        return true;
    }

    void SetMaster(Bus bus, float value) {
        master_[static_cast<std::size_t>(bus)] = FiniteUnit(value);
    }

    bool IsPlaying(std::uint64_t handle) const {
        const Slot* slot = Find(handle);
        return slot && slot->active;
    }

    std::size_t ActiveVoices() const {
        std::size_t count = 0;
        for (const auto& slot : slots_) if (slot.active) ++count;
        return count;
    }

    void CollectFinished() {
        for (auto& slot : slots_) {
            if (!slot.active) slot.pcm.reset(); // Never called by SDL callback.
        }
    }

    void Mix(std::int16_t* output, std::size_t frames) {
        if (!output) return;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const auto sample = NextFrame();
            output[frame * 2] = ToS16(sample[0]);
            output[frame * 2 + 1] = ToS16(sample[1]);
        }
    }

    // Adds unclamped voices to a caller-owned buffer. The SDL callback uses
    // this with music streams, then converts to S16 once after every source.
    void MixFloat(float* output, std::size_t frames) {
        if (!output) return;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const auto sample = NextFrame();
            output[frame * 2] += sample[0];
            output[frame * 2 + 1] += sample[1];
        }
    }

private:
    struct Slot {
        std::shared_ptr<const Pcm> pcm;
        Bus bus = Bus::effects;
        bool loop = false;
        bool active = false;
        std::uint32_t generation = 0;
        std::uint32_t fade_frames = 0;
        double position = 0.0;
        float gain = 1.0f;
        float target_gain = 1.0f;
        float fade_step = 0.0f;
        float pan = 0.0f;
    };
    std::array<Slot, kMaxVoices> slots_{};
    std::uint32_t output_rate_ = 0;
    std::array<float, 2> master_{1.0f, 1.0f};

    std::array<float, 2> NextFrame() {
        std::array<float, 2> result{};
        for (auto& slot : slots_) {
            if (!slot.active) continue;
            const auto& pcm = *slot.pcm;
            const std::size_t sample_frame = static_cast<std::size_t>(slot.position);
            const std::size_t next = sample_frame + 1 < pcm.Frames() ?
                sample_frame + 1 : (slot.loop ? 0 : sample_frame);
            const float fraction = static_cast<float>(slot.position - sample_frame);
            const float sample_left = Interpolate(pcm, sample_frame, next, 0, fraction);
            const float sample_right = pcm.channels == 2 ?
                Interpolate(pcm, sample_frame, next, 1, fraction) : sample_left;
            const float volume = slot.gain * master_[static_cast<std::size_t>(slot.bus)];
            result[0] += sample_left * volume * (slot.pan > 0.0f ? 1.0f - slot.pan : 1.0f);
            result[1] += sample_right * volume * (slot.pan < 0.0f ? 1.0f + slot.pan : 1.0f);
            if (slot.fade_frames) {
                --slot.fade_frames;
                slot.gain = slot.fade_frames ? slot.gain + slot.fade_step : slot.target_gain;
            }
            slot.position += static_cast<double>(pcm.rate) / output_rate_;
            if (slot.position >= pcm.Frames()) {
                if (slot.loop) {
                    while (slot.position >= pcm.Frames()) slot.position -= pcm.Frames();
                } else {
                    slot.active = false; // PCM ownership retained until CollectFinished.
                }
            }
        }
        return result;
    }

    static float FiniteUnit(float value) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    }
    static float FinitePan(float value) {
        return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
    }
    static std::int16_t ToS16(float value) {
        const float clamped = std::clamp(value, -1.0f, 1.0f);
        return static_cast<std::int16_t>(std::lround(clamped < 0.0f ?
            clamped * 32768.0f : clamped * 32767.0f));
    }
    static float Sample(const Pcm& pcm, std::size_t frame, std::size_t channel) {
        const std::size_t offset = (frame * pcm.channels + channel) * (pcm.bits / 8);
        if (pcm.bits == 8)
            return (static_cast<int>(pcm.bytes[offset]) - 128) / 128.0f;
        const std::uint16_t word = static_cast<std::uint16_t>(pcm.bytes[offset]) |
            (static_cast<std::uint16_t>(pcm.bytes[offset + 1]) << 8);
        return static_cast<std::int16_t>(word) / 32768.0f;
    }
    static float Interpolate(const Pcm& pcm, std::size_t a, std::size_t b,
                             std::size_t channel, float fraction) {
        const float first = Sample(pcm, a, channel);
        return first + (Sample(pcm, b, channel) - first) * fraction;
    }
    Slot* Find(std::uint64_t handle) {
        const std::uint32_t index = static_cast<std::uint32_t>(handle) - 1;
        if (index >= slots_.size()) return nullptr;
        Slot& slot = slots_[index];
        return slot.pcm && slot.generation == handle >> 32 ? &slot : nullptr;
    }
    const Slot* Find(std::uint64_t handle) const {
        return const_cast<Mixer*>(this)->Find(handle);
    }
};

} // namespace srhd_awa::platform::e2e4_audio

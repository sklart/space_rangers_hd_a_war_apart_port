#include "e2e4_sdl_audio.hpp"

#include <SDL2/SDL.h>

#include <cstring>
#include <exception>
#include <cmath>
#include <algorithm>
#include <utility>

namespace srhd_awa::platform::e2e4_audio {
namespace {
std::int16_t ToS16(float value) {
    const float limited = std::clamp(value, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lround(limited < 0.0f ?
        limited * 32768.0f : limited * 32767.0f));
}
} // namespace

bool SdlOutput::Open(std::string* error) {
    if (IsOpen()) return true;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            if (error) *error = SDL_GetError();
            return false;
        }
        owns_audio_subsystem_ = true;
    }
    SDL_AudioSpec requested{};
    requested.freq = 44100;
    requested.format = AUDIO_S16SYS;
    requested.channels = 2;
    requested.samples = 512;
    requested.callback = &SdlOutput::AudioCallback;
    requested.userdata = this;
    SDL_AudioSpec obtained{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &requested, &obtained,
                                  SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (device_ == 0 || obtained.freq <= 0 || obtained.format != AUDIO_S16SYS ||
        obtained.channels != 2) {
        if (error) *error = device_ ? "SDL audio format is not stereo S16" : SDL_GetError();
        Close();
        return false;
    }
    output_rate_ = static_cast<std::uint32_t>(obtained.freq);
    try {
        mixer_ = std::make_unique<Mixer>(output_rate_);
        mix_buffer_.resize(std::max<std::size_t>(1, obtained.samples) * 2);
    } catch (const std::exception& failure) {
        if (error) *error = failure.what();
        Close();
        return false;
    }
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void SdlOutput::Close() {
    if (device_) {
        SDL_CloseAudioDevice(device_); // Joins the callback before PCM release.
        device_ = 0;
    }
    mixer_.reset();
    mix_buffer_.clear();
    for (auto& slot : streams_) slot.ring.reset();
    output_rate_ = 0;
    if (owns_audio_subsystem_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        owns_audio_subsystem_ = false;
    }
}

std::uint64_t SdlOutput::Add(std::shared_ptr<const Pcm> pcm, Bus bus, bool loop,
                             float gain, float pan) {
    if (!device_) return 0;
    SDL_LockAudioDevice(device_);
    mixer_->CollectFinished();
    const auto handle = mixer_->Add(std::move(pcm), bus, loop, gain, pan);
    SDL_UnlockAudioDevice(device_);
    return handle;
}

bool SdlOutput::Stop(std::uint64_t handle) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    const bool stopped = mixer_->Stop(handle);
    mixer_->CollectFinished();
    SDL_UnlockAudioDevice(device_);
    return stopped;
}

bool SdlOutput::IsPlaying(std::uint64_t handle) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    const bool playing = mixer_->IsPlaying(handle);
    SDL_UnlockAudioDevice(device_);
    return playing;
}

bool SdlOutput::SetGain(std::uint64_t handle, float gain, std::uint32_t fade_frames) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    const bool changed = mixer_->SetGain(handle, gain, fade_frames);
    SDL_UnlockAudioDevice(device_);
    return changed;
}

bool SdlOutput::SetPan(std::uint64_t handle, float pan) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    const bool changed = mixer_->SetPan(handle, pan);
    SDL_UnlockAudioDevice(device_);
    return changed;
}

void SdlOutput::SetMaster(Bus bus, float gain) {
    if (!device_) return;
    SDL_LockAudioDevice(device_);
    mixer_->SetMaster(bus, gain);
    SDL_UnlockAudioDevice(device_);
}

std::size_t SdlOutput::ActiveVoices() {
    if (!device_) return 0;
    SDL_LockAudioDevice(device_);
    const auto count = mixer_->ActiveVoices();
    SDL_UnlockAudioDevice(device_);
    return count;
}

void SdlOutput::CollectFinished() {
    if (!device_) return;
    SDL_LockAudioDevice(device_);
    mixer_->CollectFinished();
    SDL_UnlockAudioDevice(device_);
}

SdlOutput::StreamSlot* SdlOutput::FindStream(std::uint64_t handle) {
    const auto index = static_cast<std::uint32_t>(handle) - 1;
    if (index >= streams_.size()) return nullptr;
    auto& slot = streams_[index];
    return slot.ring && slot.generation == handle >> 32 ? &slot : nullptr;
}

std::uint64_t SdlOutput::CreateStream(std::size_t chunk_bytes,
                                      std::uint32_t source_rate,
                                      std::uint8_t channels, std::uint8_t bits) {
    if (!device_) return 0;
    auto ring = std::make_unique<StreamRing>();
    if (!ring->Initialize(chunk_bytes, source_rate, output_rate_, channels, bits))
        return 0;
    SDL_LockAudioDevice(device_);
    std::uint64_t handle = 0;
    for (std::size_t i = 0; i < streams_.size(); ++i) {
        auto& slot = streams_[i];
        if (slot.ring) continue;
        ++slot.generation;
        if (!slot.generation) ++slot.generation;
        slot.ring = std::move(ring);
        slot.gain = 1.0f;
        handle = (static_cast<std::uint64_t>(slot.generation) << 32) | (i + 1);
        break;
    }
    SDL_UnlockAudioDevice(device_);
    return handle;
}

bool SdlOutput::WriteStreamChunk(std::uint64_t handle, std::size_t chunk,
                                 const void* data, std::size_t bytes) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    const bool written = slot && slot->ring->WriteChunk(chunk, data, bytes);
    SDL_UnlockAudioDevice(device_);
    return written;
}

bool SdlOutput::StartStream(std::uint64_t handle) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    const bool started = slot && slot->ring->Start();
    SDL_UnlockAudioDevice(device_);
    return started;
}

bool SdlOutput::StopStream(std::uint64_t handle) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    if (slot) slot->ring->Stop();
    SDL_UnlockAudioDevice(device_);
    return slot != nullptr;
}

bool SdlOutput::IsStreamPlaying(std::uint64_t handle) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    const bool playing = slot && slot->ring->IsPlaying();
    SDL_UnlockAudioDevice(device_);
    return playing;
}

bool SdlOutput::SetStreamGain(std::uint64_t handle, float gain) {
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    if (slot) slot->gain = std::isfinite(gain) ? std::clamp(gain, 0.0f, 1.0f) : 0.0f;
    SDL_UnlockAudioDevice(device_);
    return slot != nullptr;
}

std::array<std::uint32_t, StreamRing::kChunks> SdlOutput::DrainStreamCompleted(
    std::uint64_t handle) {
    if (!device_) return {};
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    const auto completed = slot ? slot->ring->DrainCompleted() :
        std::array<std::uint32_t, StreamRing::kChunks>{};
    SDL_UnlockAudioDevice(device_);
    return completed;
}

void SdlOutput::CloseStream(std::uint64_t handle) {
    if (!device_) return;
    SDL_LockAudioDevice(device_);
    auto* slot = FindStream(handle);
    std::unique_ptr<StreamRing> old;
    if (slot) old = std::move(slot->ring);
    SDL_UnlockAudioDevice(device_);
}

void SdlOutput::AudioCallback(void* userdata, std::uint8_t* stream, int bytes) {
    if (bytes <= 0 || stream == nullptr) return;
    std::memset(stream, 0, static_cast<std::size_t>(bytes));
    auto* self = static_cast<SdlOutput*>(userdata);
    if (self == nullptr || !self->mixer_ || self->mix_buffer_.empty()) return;
    auto* samples = reinterpret_cast<std::int16_t*>(stream);
    const std::size_t frames = static_cast<std::size_t>(bytes) /
        (2 * sizeof(std::int16_t));
    const std::size_t capacity = self->mix_buffer_.size() / 2;
    for (std::size_t offset = 0; offset < frames;) {
        const auto count = std::min(capacity, frames - offset);
        auto* mixed = self->mix_buffer_.data();
        std::fill_n(mixed, count * 2, 0.0f);
        self->mixer_->MixFloat(mixed, count);
        for (auto& slot : self->streams_) {
            if (slot.ring) slot.ring->RenderAddFloat(mixed, count, slot.gain);
        }
        for (std::size_t i = 0; i < count * 2; ++i)
            samples[offset * 2 + i] = ToS16(mixed[i]);
        offset += count;
    }
}

} // namespace srhd_awa::platform::e2e4_audio

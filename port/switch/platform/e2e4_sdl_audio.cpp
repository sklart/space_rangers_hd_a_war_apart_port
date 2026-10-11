#include "e2e4_sdl_audio.hpp"

#include <SDL2/SDL.h>

#include <cstring>
#include <exception>
#include <utility>

namespace srhd_awa::platform::e2e4_audio {

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

void SdlOutput::AudioCallback(void* userdata, std::uint8_t* stream, int bytes) {
    if (bytes <= 0 || stream == nullptr) return;
    std::memset(stream, 0, static_cast<std::size_t>(bytes));
    auto* self = static_cast<SdlOutput*>(userdata);
    if (self == nullptr || !self->mixer_) return;
    self->mixer_->Mix(reinterpret_cast<std::int16_t*>(stream),
                      static_cast<std::size_t>(bytes) / (2 * sizeof(std::int16_t)));
}

} // namespace srhd_awa::platform::e2e4_audio

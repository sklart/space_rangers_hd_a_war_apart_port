#pragma once

#include "e2e4_audio_mixer.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace srhd_awa::platform::e2e4_audio {

// Narrow SDL2 output device for the PCM mixer. Game/DirectSound integration
// must explicitly create and use this object; an Audio ON build is not enabled
// merely by compiling it.
class SdlOutput {
public:
    SdlOutput() = default;
    ~SdlOutput() { Close(); }
    SdlOutput(const SdlOutput&) = delete;
    SdlOutput& operator=(const SdlOutput&) = delete;

    bool Open(std::string* error);
    void Close();
    bool IsOpen() const { return device_ != 0; }
    std::uint32_t OutputRate() const { return output_rate_; }
    std::uint64_t Add(std::shared_ptr<const Pcm> pcm, Bus bus, bool loop,
                      float gain = 1.0f, float pan = 0.0f);
    bool Stop(std::uint64_t handle);
    bool SetGain(std::uint64_t handle, float gain, std::uint32_t fade_frames = 0);
    bool SetPan(std::uint64_t handle, float pan);
    void SetMaster(Bus bus, float gain);
    std::size_t ActiveVoices();
    void CollectFinished();

private:
    static void AudioCallback(void* userdata, std::uint8_t* stream, int bytes);
    std::unique_ptr<Mixer> mixer_;
    std::uint32_t device_ = 0;
    std::uint32_t output_rate_ = 0;
    bool owns_audio_subsystem_ = false;
};

} // namespace srhd_awa::platform::e2e4_audio

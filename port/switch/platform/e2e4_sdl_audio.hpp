#pragma once

#include "e2e4_audio_mixer.hpp"
#include "e2e4_audio_stream.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace srhd_awa::platform::e2e4_audio {

struct StreamStats {
    bool valid = false;
    std::uint64_t played_frames = 0;
    std::uint64_t underrun_frames = 0;
    std::uint64_t rejected_writes = 0;
};

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
    bool IsPlaying(std::uint64_t handle);
    bool SetGain(std::uint64_t handle, float gain, std::uint32_t fade_frames = 0);
    bool SetPan(std::uint64_t handle, float pan);
    void SetMaster(Bus bus, float gain);
    std::size_t ActiveVoices();
    void CollectFinished();
    std::uint64_t CreateStream(std::size_t chunk_bytes, std::uint32_t source_rate,
                               std::uint8_t channels, std::uint8_t bits);
    bool WriteStreamChunk(std::uint64_t handle, std::size_t chunk,
                          const void* data, std::size_t bytes);
    bool StartStream(std::uint64_t handle);
    bool StopStream(std::uint64_t handle);
    bool IsStreamPlaying(std::uint64_t handle);
    bool SetStreamGain(std::uint64_t handle, float gain);
    std::array<std::uint32_t, StreamRing::kChunks> DrainStreamCompleted(
        std::uint64_t handle);
    StreamStats GetStreamStats(std::uint64_t handle);
    void CloseStream(std::uint64_t handle);

private:
    static void AudioCallback(void* userdata, std::uint8_t* stream, int bytes);
    std::unique_ptr<Mixer> mixer_;
    std::vector<float> mix_buffer_;
    struct StreamSlot {
        std::unique_ptr<StreamRing> ring;
        std::uint32_t generation = 0;
        float gain = 1.0f;
    };
    static constexpr std::size_t kMaxStreams = 2;
    std::array<StreamSlot, kMaxStreams> streams_{};
    StreamSlot* FindStream(std::uint64_t handle);
    std::uint32_t device_ = 0;
    std::uint32_t output_rate_ = 0;
    bool owns_audio_subsystem_ = false;
};

} // namespace srhd_awa::platform::e2e4_audio

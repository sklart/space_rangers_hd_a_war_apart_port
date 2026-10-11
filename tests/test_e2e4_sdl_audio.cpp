#include "e2e4_sdl_audio.hpp"
#include "e2e4_game_audio_bridge.hpp"

#include <SDL2/SDL.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using srhd_awa::platform::e2e4_audio::SdlOutput;

int main() {
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SdlOutput output;
    std::string error;
    assert(output.Open(&error) && error.empty());
    assert(output.OutputRate() > 0);
    constexpr std::uint8_t sample[4]{0, 64, 0, 64};
    const auto stream = output.CreateStream(4, 44100, 2, 16);
    assert(stream != 0);
    assert(!output.StartStream(stream));
    for (std::size_t chunk = 0; chunk < 3; ++chunk)
        assert(output.WriteStreamChunk(stream, chunk, sample, sizeof(sample)));
    assert(output.SetStreamGain(stream, 0.5f));
    assert(output.StartStream(stream));
    bool advanced = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto completed = output.DrainStreamCompleted(stream);
        if (completed[0] && completed[1] && completed[2]) {
            advanced = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(advanced);
    const auto stream_stats = output.GetStreamStats(stream);
    assert(stream_stats.valid && stream_stats.played_frames > 0);
    assert(stream_stats.underrun_frames > 0); // No worker refilled the 1-frame chunks.
    assert(output.StopStream(stream));
    output.CloseStream(stream);
    assert(!output.StartStream(stream));
    output.Close();

    namespace bridge = srhd_awa::platform::e2e4_game_audio;
    assert(bridge::Open(&error));
    const bridge::Format format{1, 2, 44100, 176400, 4, 16};
    std::vector<std::uint8_t> effect(176400, 0);
    int effect_owner = 0;
    assert(bridge::PrepareEffect(&effect_owner, effect.data(), effect.size(), format));
    bridge::SetPan(&effect_owner, -0.5f);
    assert(bridge::Play(&effect_owner, false, 0.5f));
    assert(bridge::IsPlaying(&effect_owner));
    bridge::Stop(&effect_owner);
    bridge::Clear(&effect_owner);

    int music_owner = 0;
    assert(bridge::PrepareStream(&music_owner, 4, format));
    for (std::size_t chunk = 0; chunk < 3; ++chunk)
        assert(bridge::WriteStreamChunk(&music_owner, chunk, sample, sizeof(sample)));
    assert(bridge::Play(&music_owner, true, 1.0f));
    int first_event = -1;
    for (int attempt = 0; attempt < 100 && first_event < 0; ++attempt) {
        first_event = bridge::PollCompletion(&music_owner);
        if (first_event < 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(first_event == 1); // Original event 1 follows completed chunk 0.
    const auto bridge_stats = bridge::GetStreamStats(&music_owner);
    assert(bridge_stats.valid && bridge_stats.played_frames > 0);
    bridge::Clear(&music_owner);

    int deferred_owner = 0;
    constexpr std::size_t slow_chunk_bytes = 176400; // One second at stereo S16/44.1 kHz.
    std::vector<std::uint8_t> slow_chunk(slow_chunk_bytes, 0);
    assert(bridge::PrepareStream(&deferred_owner, slow_chunk_bytes, format));
    for (std::size_t chunk = 0; chunk < 3; ++chunk)
        assert(bridge::WriteStreamChunk(&deferred_owner, chunk, slow_chunk.data(), slow_chunk.size()));
    assert(bridge::Play(&deferred_owner, true, 1.0f));
    assert(!bridge::WriteStreamChunk(&deferred_owner, 0, slow_chunk.data(), slow_chunk.size()));
    assert(bridge::DeferStreamChunk(&deferred_owner, 0, slow_chunk.data(), slow_chunk.size()));
    assert(bridge::RetryDeferredStreamChunk(&deferred_owner) == bridge::DeferredWriteResult::Pending);
    auto retry = bridge::DeferredWriteResult::Pending;
    for (int attempt = 0; attempt < 150 && retry == bridge::DeferredWriteResult::Pending; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        retry = bridge::RetryDeferredStreamChunk(&deferred_owner);
    }
    assert(retry == bridge::DeferredWriteResult::Written);
    assert(bridge::RetryDeferredStreamChunk(&deferred_owner) == bridge::DeferredWriteResult::None);
    bridge::Clear(&deferred_owner);
    bridge::Close();
}

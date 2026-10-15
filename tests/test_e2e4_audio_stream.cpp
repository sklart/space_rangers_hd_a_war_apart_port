#include "e2e4_audio_stream.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using srhd_awa::platform::e2e4_audio::StreamRing;

int main() {
    StreamRing ring;
    assert(!ring.Initialize(0, 44100, 44100, 2, 16));
    assert(!ring.Initialize(7, 44100, 44100, 2, 16));
    assert(!ring.Initialize(StreamRing::kMaxBytes, 44100, 44100, 2, 16));
    assert(ring.Initialize(352800, 44100, 44100, 2, 16));
    assert(ring.Initialize(4, 44100, 44100, 2, 16));
    assert(!ring.Start());
    assert(!ring.IsPrimed());
    assert(ring.ReadyChunks() == 0);

    const std::array<std::uint8_t, 4> left{0x00, 0x40, 0x00, 0x40};
    const std::array<std::uint8_t, 4> right{0x00, 0x20, 0x00, 0x20};
    assert(ring.WriteChunk(0, left.data(), left.size()));
    assert(ring.WriteChunk(1, right.data(), right.size()));
    assert(ring.WriteChunk(2, left.data(), left.size()));
    assert(ring.IsPrimed());
    assert(ring.ReadyChunks() == 3);
    assert(ring.Start());
    assert(!ring.IsPrimed());
    assert(!ring.WriteChunk(0, right.data(), right.size()));
    assert(ring.RejectedWrites() == 1);

    std::array<std::int16_t, 2> mixed{};
    ring.RenderAdd(mixed.data(), 1);
    assert(mixed[0] == 16384 && mixed[1] == 16384);
    assert(ring.PlayCursorBytes() == 4 && ring.CurrentChunk() == 1);
    assert(ring.ReadyChunks() == 2);
    auto events = ring.DrainCompleted();
    assert(events[0] == 1 && events[1] == 0 && events[2] == 0);
    assert(ring.WriteChunk(0, right.data(), right.size()));

    mixed = {};
    ring.RenderAdd(mixed.data(), 1);
    assert(mixed[0] == 8192 && mixed[1] == 8192);
    events = ring.DrainCompleted();
    assert(events[1] == 1);
    ring.RenderAdd(mixed.data(), 1);
    ring.RenderAdd(mixed.data(), 1);
    events = ring.DrainCompleted();
    assert(events[2] == 1 && events[0] == 1);
    assert(ring.PlayedFrames() == 4);
    mixed = {};
    ring.RenderAdd(mixed.data(), 1); // Missing refill is silence, not old PCM.
    assert(mixed[0] == 0 && mixed[1] == 0);
    assert(ring.UnderrunFrames() == 1);
    assert(ring.ReadyChunks() == 0);
    ring.Stop();
    assert(!ring.IsPlaying());
    assert(!ring.IsPrimed()); // A stopped old track cannot become queued again.

    // Rate conversion crosses multiple chunks in one output frame and never
    // loses notifications. Unsigned 8-bit silence is 128, not zero.
    assert(ring.Initialize(1, 192000, 8000, 1, 8));
    const std::uint8_t sample = 255;
    for (std::size_t i = 0; i < 3; ++i) assert(ring.WriteChunk(i, &sample, 1));
    assert(ring.Start());
    mixed = {};
    ring.RenderAdd(mixed.data(), 1);
    events = ring.DrainCompleted();
    assert(events[0] == 8 && events[1] == 8 && events[2] == 8);
    assert(mixed[0] > 32000 && mixed[1] > 32000);
    assert(ring.UnderrunFrames() == 0 && ring.RejectedWrites() == 0);

    // Delayed worker refill: a full revolution may create multiple completed
    // events and audible silence.  All events remain available for recovery.
    assert(ring.Initialize(40, 10, 10, 2, 16));
    std::array<std::uint8_t, 40> ten_frames{};
    for (std::size_t i = 0; i < 3; ++i)
        assert(ring.WriteChunk(i, ten_frames.data(), ten_frames.size()));
    assert(ring.Start());
    std::array<std::int16_t, 80> delayed{};
    ring.RenderAdd(delayed.data(), 40);
    assert(ring.UnderrunFrames() == 10);
    events = ring.DrainCompleted();
    assert(events[0] == 2 && events[1] == 1 && events[2] == 1);
    assert(!ring.WriteChunk(1, ten_frames.data(), ten_frames.size()));
    assert(ring.RejectedWrites() == 1);
}

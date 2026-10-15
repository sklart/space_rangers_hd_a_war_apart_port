#include "e2e4_audio_mixer.hpp"
#include "e2e4_audio_stream.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using srhd_awa::platform::e2e4_audio::Bus;
using srhd_awa::platform::e2e4_audio::Mixer;
using srhd_awa::platform::e2e4_audio::Pcm;
using srhd_awa::platform::e2e4_audio::StreamRing;

static std::shared_ptr<Pcm> S16(std::uint32_t rate, std::uint8_t channels,
                                std::initializer_list<std::int16_t> samples) {
    auto pcm = std::make_shared<Pcm>();
    pcm->rate = rate;
    pcm->channels = channels;
    pcm->bits = 16;
    for (const auto sample : samples) {
        pcm->bytes.push_back(static_cast<std::uint8_t>(sample));
        pcm->bytes.push_back(static_cast<std::uint8_t>(sample >> 8));
    }
    return pcm;
}

static bool Near(int actual, int expected, int tolerance = 2) {
    return std::abs(actual - expected) <= tolerance;
}

int main() {
    Mixer mixer(44100);
    auto mono8 = std::make_shared<Pcm>();
    mono8->rate = 44100;
    mono8->channels = 1;
    mono8->bits = 8;
    mono8->bytes = {0, 128, 255};
    const auto first = mixer.Add(mono8, Bus::effects, false);
    std::array<std::int16_t, 6> result{};
    mixer.Mix(result.data(), 3);
    assert(result[0] == -32768 && result[1] == -32768);
    assert(result[2] == 0 && result[3] == 0);
    assert(Near(result[4], 32511) && result[4] == result[5]);
    assert(!mixer.IsPlaying(first) && mono8.use_count() == 2);
    mixer.CollectFinished();
    assert(mono8.use_count() == 1);

    auto stereo = S16(44100, 2, {16384, -16384});
    const auto panned = mixer.Add(stereo, Bus::effects, true, 1.0f, 1.0f);
    mixer.Mix(result.data(), 1);
    assert(result[0] == 0 && Near(result[1], -16384));
    assert(mixer.SetPan(panned, -1.0f));
    mixer.Mix(result.data(), 1);
    assert(Near(result[0], 16384) && result[1] == 0);
    mixer.SetMaster(Bus::effects, 0.5f);
    mixer.Mix(result.data(), 1);
    assert(Near(result[0], 8192) && result[1] == 0);
    assert(mixer.Stop(panned));
    mixer.CollectFinished();
    assert(!mixer.IsPlaying(panned));

    mixer.SetMaster(Bus::effects, 1.0f);
    auto slow = S16(22050, 1, {0, 32767});
    const auto resampled = mixer.Add(slow, Bus::music, false);
    std::array<std::int16_t, 8> resample{};
    mixer.Mix(resample.data(), 4);
    assert(resample[0] == 0 && Near(resample[2], 16383));
    assert(Near(resample[4], 32766) && Near(resample[6], 32766));
    assert(!mixer.IsPlaying(resampled));
    mixer.CollectFinished();

    auto loud = S16(48000, 1, {30000, 30000, 30000, 30000});
    const auto voice1 = mixer.Add(loud, Bus::effects, true);
    const auto voice2 = mixer.Add(loud, Bus::music, true);
    mixer.Mix(result.data(), 1);
    assert(result[0] == 32767 && result[1] == 32767); // clipped sum
    mixer.SetMaster(Bus::music, 0.0f);
    mixer.Mix(result.data(), 1);
    assert(Near(result[0], 30000));
    mixer.SetMaster(Bus::effects, 0.0f);
    mixer.Mix(result.data(), 1);
    assert(result[0] == 0 && result[1] == 0);
    mixer.SetMaster(Bus::effects, 1.0f);
    assert(mixer.SetGain(voice1, 0.0f, 2));
    mixer.Mix(result.data(), 3);
    assert(Near(result[0], 30000) && Near(result[2], 15000) && result[4] == 0);
    mixer.Stop(voice1);
    mixer.Stop(voice2);
    mixer.CollectFinished();

    const auto newer = mixer.Add(loud, Bus::effects, false);
    assert(newer != voice1 && !mixer.Stop(voice1)); // stale handle cannot stop reuse
    assert(mixer.IsPlaying(newer));
    mixer.Stop(newer);
    mixer.CollectFinished();

    auto invalid = S16(0, 1, {1});
    assert(!mixer.Add(invalid, Bus::effects, false));
    invalid->rate = 44100;
    invalid->bytes.pop_back();
    assert(!mixer.Add(invalid, Bus::effects, false));

    // Opposite-phase music must cancel part of two loud effects before the
    // final S16 clamp. Clamping effects first would produce about 0.25 here.
    Mixer combined(44100);
    auto positive = S16(44100, 2, {24576, 24576});
    assert(combined.Add(positive, Bus::effects, false));
    assert(combined.Add(positive, Bus::effects, false));
    StreamRing music;
    assert(music.Initialize(4, 44100, 44100, 2, 16));
    const std::array<std::uint8_t, 4> negative{0, 0xa0, 0, 0xa0};
    for (std::size_t chunk = 0; chunk < 3; ++chunk)
        assert(music.WriteChunk(chunk, negative.data(), negative.size()));
    assert(music.Start());
    std::array<float, 2> sum{};
    combined.MixFloat(sum.data(), 1);
    music.RenderAddFloat(sum.data(), 1);
    assert(std::abs(sum[0] - 0.75f) < 0.001f);
    assert(std::abs(sum[1] - 0.75f) < 0.001f);
}

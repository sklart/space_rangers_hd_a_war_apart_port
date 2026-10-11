#include "e2e4_wav.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using srhd_awa::platform::e2e4_wav::Parse;

static void Put16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

static void Put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    Put16(out, static_cast<std::uint16_t>(v));
    Put16(out, static_cast<std::uint16_t>(v >> 16));
}

static void Chunk(std::vector<std::uint8_t>& out, const char* tag,
                  const std::vector<std::uint8_t>& body) {
    for (int i = 0; i < 4; ++i) out.push_back(tag[i]);
    Put32(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    if (body.size() & 1u) out.push_back(0);
}

static std::vector<std::uint8_t> Wave(std::uint16_t channels, std::uint16_t bits,
                                      std::uint32_t rate,
                                      const std::vector<std::uint8_t>& pcm,
                                      bool extra = false) {
    std::vector<std::uint8_t> out{'R', 'I', 'F', 'F', 0, 0, 0, 0,
                                  'W', 'A', 'V', 'E'};
    std::vector<std::uint8_t> fmt;
    const auto align = static_cast<std::uint16_t>(channels * (bits / 8));
    Put16(fmt, 1);
    Put16(fmt, channels);
    Put32(fmt, rate);
    Put32(fmt, rate * align);
    Put16(fmt, align);
    Put16(fmt, bits);
    Chunk(out, "fmt ", fmt);
    if (extra) Chunk(out, "JUNK", {0x64, 0x61, 0x74, 0x61, 0x7f});
    Chunk(out, "data", pcm);
    const auto size = static_cast<std::uint32_t>(out.size() - 8);
    for (int i = 0; i < 4; ++i) out[4 + i] = static_cast<std::uint8_t>(size >> (8 * i));
    return out;
}

int main() {
    for (const auto& fixture : {
             Wave(1, 8, 11025, {0, 128, 255}, true),
             Wave(1, 16, 22050, {0, 0, 255, 127}),
             Wave(2, 16, 44100, {1, 0, 2, 0, 3, 0, 4, 0}),
             Wave(2, 16, 48000, {0, 0, 0, 0}),
         }) {
        const auto result = Parse(fixture.data(), fixture.size());
        assert(result);
        assert(result.next_position <= fixture.size());
        assert(result.bytes % result.block_align == 0);
        assert(result.rate > 0);
        // The returned span is the actual original PCM, including the byte
        // pattern in the JUNK chunk that mimics a false data identifier.
        assert(fixture[result.offset] != 'd' || fixture[result.offset + 1] != 'a');
    }
    auto wave = Wave(1, 8, 44100, {0, 128, 255}, true);
    auto result = Parse(wave.data(), wave.size());
    assert(result && result.bytes == 3);
    assert(wave[result.offset] == 0 && wave[result.offset + 1] == 128 &&
           wave[result.offset + 2] == 255);
    wave[0] = 'X';
    assert(!Parse(wave.data(), wave.size()));
    wave[0] = 'R';
    wave.pop_back();
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1, 2, 3});
    wave[16] = 0xff; // fmt chunk exceeds RIFF bounds.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(2, 16, 44100, {1, 0, 2}); // Partial stereo frame.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 16, 0, {0, 0});
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1});
    wave[20] = 3; // IEEE float, not PCM.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1});
    wave[32] = 2; // Invalid block alignment.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1});
    wave[36] = 'J'; // No data chunk.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1});
    wave[12] = 'J'; // No fmt chunk.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1});
    wave[40] = 0xff; // Oversized data chunk.
    assert(!Parse(wave.data(), wave.size()));
    wave = Wave(1, 8, 44100, {1});
    std::vector<std::uint8_t> prefixed{9, 8, 7};
    prefixed.insert(prefixed.end(), wave.begin(), wave.end());
    result = Parse(prefixed.data(), prefixed.size(), 3);
    assert(result && result.offset == 3 + 44 && prefixed[result.offset] == 1);
    // Deterministic malformed-input sweep: parser must never form a span
    // outside the supplied buffer.
    std::uint32_t seed = 0x61a5b77u;
    for (std::size_t length = 0; length < 256; ++length) {
        std::vector<std::uint8_t> bytes(length);
        for (auto& byte : bytes) {
            seed = seed * 1664525u + 1013904223u;
            byte = static_cast<std::uint8_t>(seed >> 24);
        }
        const auto parsed = Parse(bytes.data(), bytes.size());
        if (parsed) {
            assert(parsed.offset <= bytes.size());
            assert(parsed.bytes <= bytes.size() - parsed.offset);
        }
    }
}

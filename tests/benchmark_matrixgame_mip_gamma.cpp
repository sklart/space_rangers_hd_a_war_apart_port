#include "texture_asset.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>

namespace mg = srhd_awa::matrixgame;
using Clock = std::chrono::steady_clock;

mg::TextureAsset Make(unsigned width, unsigned height, unsigned gamma, unsigned padding) {
    mg::TextureAsset image;
    image.png_gamma = gamma;
    image.clear_pixel = {17, 83, 149, 211};
    const unsigned pitch = width * 4 + padding;
    mg::TextureLevel level{width, height, pitch, std::vector<std::byte>(std::size_t(pitch) * height)};
    std::uint32_t state = 0x719f8123;
    for (auto& byte : level.bytes) {
        state = state * 1664525u + 1013904223u;
        byte = std::byte(state >> 24);
    }
    image.levels.push_back(std::move(level));
    return image;
}

long long Measure(const mg::TextureAsset& input, mg::TextureMipPolicy policy,
                  unsigned repeats, std::uint64_t& hash) {
    std::chrono::nanoseconds elapsed{};
    for (unsigned i = 0; i < repeats; ++i) {
        auto texture = input;
        const auto start = Clock::now();
        texture.PrepareMips(policy);
        elapsed += Clock::now() - start;
        assert(texture.png_gamma == 0);
        for (const auto& level : texture.levels) {
            for (std::size_t offset = 0; offset < level.bytes.size(); offset += 4093) {
                hash = (hash ^ unsigned(level.bytes[offset])) * 1099511628211ull;
            }
        }
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

int main() {
    const auto padded = Make(1023, 767, 45455, 13);
    const auto power_of_two = Make(1024, 1024, 60000, 0);
    const auto plain = Make(1023, 767, 0, 13);
    const auto box = Make(512, 256, 0, 0);
    std::uint64_t hash = 1469598103934665603ull;
    Measure(padded, mg::TextureMipPolicy::OneLevel, 4, hash);
    const auto gamma_padded = Measure(padded, mg::TextureMipPolicy::OneLevel, 128, hash);
    const auto gamma_pot = Measure(power_of_two, mg::TextureMipPolicy::CompleteBox, 128, hash);
    const auto copy = Measure(plain, mg::TextureMipPolicy::OneLevel, 128, hash);
    const auto box_time = Measure(box, mg::TextureMipPolicy::CompleteBox, 128, hash);
    std::printf("gamma_padded_us=%lld gamma_pot_us=%lld copy_us=%lld box_us=%lld checksum=%llu\n",
        gamma_padded, gamma_pot, copy, box_time, static_cast<unsigned long long>(hash));
}

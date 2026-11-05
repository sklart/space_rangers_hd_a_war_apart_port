#include <cassert>
#include <chrono>
#include <cstdio>

namespace mg = srhd_awa::matrixgame;
namespace nt = mg::native_texture;
using Clock = std::chrono::steady_clock;

mg::TextureAsset MakeRgba(unsigned width, unsigned height, unsigned padding, unsigned levels) {
    mg::TextureAsset asset;
    unsigned state = 0x21fa90b5;
    for (unsigned i = 0; i < levels; ++i) {
        const unsigned pitch = width * 4 + padding;
        // There are no padding bytes after the final input row.
        mg::TextureLevel level{width, height, pitch,
            std::vector<std::byte>(std::size_t(pitch) * (height - 1) + width * 4)};
        for (auto& byte : level.bytes) {
            state = state * 1664525u + 1013904223u;
            byte = std::byte(state >> 24);
        }
        asset.levels.push_back(std::move(level));
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }
    return asset;
}

void CheckPixels(const mg::TextureAsset& input, const nt::Texture& output, unsigned format) {
    assert(output.format == format && output.levels.size() == input.levels.size());
    nt::ValidateUpload(output);
    for (unsigned i = 0; i < input.levels.size(); ++i) {
        const auto& source = input.levels[i];
        const auto& target = output.levels[i];
        assert(source.width == target.width && source.height == target.height);
        for (unsigned y = 0; y < source.height; ++y) {
            const auto* src = source.bytes.data() + std::size_t(y) * source.pitch;
            const auto* dst = target.bytes.data() + std::size_t(y) * target.pitch;
            for (unsigned x = 0; x < source.width; ++x, src += 4) {
                const auto r = unsigned(src[0]), g = unsigned(src[1]);
                const auto b = unsigned(src[2]), a = unsigned(src[3]);
                if (format == 21 || format == 22) {
                    assert(unsigned(dst[4*x]) == b && unsigned(dst[4*x+1]) == g);
                    assert(unsigned(dst[4*x+2]) == r && unsigned(dst[4*x+3]) == (format == 21 ? a : 255));
                } else {
                    std::uint16_t word;
                    std::memcpy(&word, dst + 2*x, sizeof(word));
                    if (format == 23) {
                        assert(word / 2048 == r / 8 && word / 32 % 64 == g / 4 && word % 32 == b / 8);
                    } else if (format == 25) {
                        assert(word / 32768 == a / 128 && word / 1024 % 32 == r / 8);
                        assert(word / 32 % 32 == g / 8 && word % 32 == b / 8);
                    } else {
                        assert(format == 26 && word / 4096 == a / 16 && word / 256 % 16 == r / 16);
                        assert(word / 16 % 16 == g / 16 && word % 16 == b / 16);
                    }
                }
            }
        }
    }
}

int main(int argc, char**) {
    auto session = std::make_shared<nt::Session>();
    session->upload = [](const nt::Texture&) -> std::shared_ptr<nt::Image> {
        std::terminate(); // Pool 2 is system memory. Any GPU request is a test failure.
    };
    const auto handles = nt::HandleCount();
    if (argc == 1) {
        unsigned cases = 0;
        for (unsigned width : {1u, 2u, 7u, 16u, 63u}) {
            for (unsigned height : {1u, 2u, 9u, 32u}) {
                for (unsigned padding : {0u, 1u, 13u}) {
                    for (unsigned format : {0u, 21u, 22u, 23u, 25u, 26u}) {
                        const auto input = MakeRgba(width, height, padding, width > 1 || height > 1 ? 2 : 1);
                        IDirect3DTexture9* token = nullptr;
                        assert(nt::Publish(session, input, format, 2, &token) == 0 && token);
                        CheckPixels(input, *nt::Find(token), format ? format : 21);
                        nt::Release(token);
                        ++cases;
                    }
                }
            }
        }
        assert(nt::HandleCount() == handles);
        std::printf("PUBLISH_PIXELS cases=%u all_channels all_levels exact_stride no_GPU\n", cases);
        return 0;
    }

    const auto input = MakeRgba(1024, 1024, 13, 5);
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned format : {21u, 22u, 23u, 25u, 26u}) {
        std::chrono::nanoseconds elapsed{};
        for (unsigned i = 0; i < 48; ++i) {
            auto asset = input;
            IDirect3DTexture9* token = nullptr;
            const auto begin = Clock::now();
            assert(nt::Publish(session, std::move(asset), format, 2, &token) == 0);
            elapsed += Clock::now() - begin;
            for (const auto& level : nt::Find(token)->levels) {
                for (std::size_t offset = 0; offset < level.bytes.size(); offset += 4093) {
                    hash = (hash ^ unsigned(level.bytes[offset])) * 1099511628211ull;
                }
            }
            nt::Release(token);
        }
        std::printf("f%u_us=%lld ", format, static_cast<long long>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()));
    }
    assert(nt::HandleCount() == handles);
    std::printf("checksum=%llu\n", static_cast<unsigned long long>(hash));
}

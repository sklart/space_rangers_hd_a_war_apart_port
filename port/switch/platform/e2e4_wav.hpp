#pragma once

#include <cstddef>
#include <cstdint>

namespace srhd_awa::platform::e2e4_wav {

inline constexpr std::size_t kMaxPcmBytes = 64u * 1024u * 1024u;

struct WaveData {
    const char* error = "invalid RIFF/WAVE";
    std::size_t offset = 0;
    std::size_t bytes = 0;
    std::size_t next_position = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits = 0;
    std::uint16_t block_align = 0;
    std::uint32_t rate = 0;
    explicit operator bool() const { return error == nullptr; }
};

inline std::uint16_t Read16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<unsigned>(p[1]) << 8));
}

inline std::uint32_t Read32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

inline bool FourCC(const std::uint8_t* p, const char* id) {
    return p[0] == static_cast<std::uint8_t>(id[0]) &&
           p[1] == static_cast<std::uint8_t>(id[1]) &&
           p[2] == static_cast<std::uint8_t>(id[2]) &&
           p[3] == static_cast<std::uint8_t>(id[3]);
}

// Offsets are relative to the complete source buffer, whose current position
// may point at a RIFF object embedded in a game package.
inline WaveData Parse(const void* buffer, std::size_t length, std::size_t start = 0) {
    WaveData out;
    if (buffer == nullptr || start > length || length - start < 12) return out;
    const auto* data = static_cast<const std::uint8_t*>(buffer);
    if (!FourCC(data + start, "RIFF") || !FourCC(data + start + 8, "WAVE")) return out;
    const std::size_t riff_bytes = Read32(data + start + 4);
    if (riff_bytes < 4 || riff_bytes > length - start - 8) {
        out.error = "truncated RIFF";
        return out;
    }
    const std::size_t end = start + 8 + riff_bytes;
    bool have_fmt = false;
    bool have_data = false;
    for (std::size_t pos = start + 12; pos < end;) {
        if (end - pos < 8) {
            out.error = "truncated chunk header";
            return out;
        }
        const std::size_t bytes = Read32(data + pos + 4);
        const std::size_t payload = pos + 8;
        if (bytes > end - payload) {
            out.error = "truncated chunk";
            return out;
        }
        if (FourCC(data + pos, "fmt ") && !have_fmt) {
            if (bytes < 16) {
                out.error = "short fmt chunk";
                return out;
            }
            const auto format = Read16(data + payload);
            out.channels = Read16(data + payload + 2);
            out.rate = Read32(data + payload + 4);
            const auto byte_rate = Read32(data + payload + 8);
            out.block_align = Read16(data + payload + 12);
            out.bits = Read16(data + payload + 14);
            if (format != 1 || (out.channels != 1 && out.channels != 2) ||
                (out.bits != 8 && out.bits != 16) || out.rate == 0 ||
                out.rate > 192000 ||
                out.block_align != out.channels * (out.bits / 8) ||
                byte_rate != static_cast<std::uint64_t>(out.rate) * out.block_align) {
                out.error = "unsupported or inconsistent PCM fmt";
                return out;
            }
            have_fmt = true;
        } else if (FourCC(data + pos, "data") && !have_data) {
            if (bytes == 0 || bytes > kMaxPcmBytes) {
                out.error = "invalid PCM data size";
                return out;
            }
            out.offset = payload;
            out.bytes = bytes;
            out.next_position = payload + bytes;
            have_data = true;
        }
        const std::size_t padded = bytes + (bytes & 1u);
        if (padded > end - payload) {
            out.error = "truncated chunk padding";
            return out;
        }
        pos = payload + padded;
    }
    if (!have_fmt || !have_data) {
        out.error = have_fmt ? "missing data chunk" : "missing fmt chunk";
        return out;
    }
    if (out.bytes % out.block_align != 0) {
        out.error = "partial PCM frame";
        return out;
    }
    out.error = nullptr;
    return out;
}

} // namespace srhd_awa::platform::e2e4_wav

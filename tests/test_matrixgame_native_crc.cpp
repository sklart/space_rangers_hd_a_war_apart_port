// Tests the original MatrixLib CRC implementation, including its nonstandard
// left-shift update. Do not replace it with zlib's CRC or alter saved semantics.
#include "CRC32.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

#ifdef NDEBUG
#error CRC contract requires active assertions
#endif

static_assert(sizeof(dword) == 4 && sizeof(word) == 2 && sizeof(byte) == 1);
static_assert(std::numeric_limits<dword>::is_integer && !std::numeric_limits<dword>::is_signed);
struct DiskPair { dword crc; dword size; };
static_assert(sizeof(DiskPair) == 8);

std::uint32_t reference(std::span<const byte> bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (byte b : bytes) {
        std::uint32_t entry = (crc & 255u) ^ b;
        for (int bit = 0; bit < 8; ++bit)
            entry = (entry >> 1) ^ ((entry & 1u) ? 0xedb88320u : 0u);
        crc = (crc << 8) ^ entry;
    }
    return ~crc;
}

int main() {
    std::vector<byte> bytes(4096);
    std::uint32_t rng = 0x17ac942bu;
    for (byte& b : bytes) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        b = static_cast<byte>(rng);
    }
    assert(Base::CalcCRC32(nullptr, 0) == 0);
    assert(Base::CalcCRC32_End(Base::CalcCRC32_Begin(nullptr, 0)) == 0);
    std::uint32_t transcript = 2166136261u;
    unsigned checked = 0;
    for (std::size_t length = 0; length <= bytes.size(); ++length) {
        const auto data = std::span<const byte>(bytes.data(), length);
        const auto expected = reference(data);
        assert(Base::CalcCRC32(data.data(), static_cast<int>(length)) == expected);
        // Split at boundaries independent of the implementation, including zero.
        for (const std::size_t split : {std::size_t(0), length / 3, length / 2, length}) {
            auto state = Base::CalcCRC32_Begin(nullptr, 0);
            state = Base::CalcCRC32_Buf(state, data.data(), static_cast<int>(split));
            state = Base::CalcCRC32_Buf(state, data.data() + split, static_cast<int>(length - split));
            assert(Base::CalcCRC32_End(state) == expected);
            ++checked;
        }
        for (int shift = 0; shift < 32; shift += 8)
            transcript = (transcript ^ ((expected >> shift) & 255u)) * 16777619u;
    }
    // Golden transcript obtained from the original MSVC x86 build.
    assert(transcript == 0x7a4520ffu);
    std::printf("{\"lengths\":4097,\"split_checks\":%u,\"transcript_fnv32\":\"%08x\"}\n", checked, transcript);
}

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace srhd_awa::platform::e2e4_vorbis_read {

inline constexpr int kScratchBytes = 4096;
inline constexpr int kFrameBytes = 4; // Original output: stereo signed 16-bit.

struct Result {
    int bytes = 0;
    int error = 0;
    bool end_of_stream = false;
};

// Reader receives a scratch pointer and a length no larger than its capacity.
// A negative result is an actual decoder failure, not end of stream.
template <class Reader>
Result ReadBlocks(void* output, int wanted, Reader&& reader) {
    Result result;
    if (output == nullptr || wanted < 0 || wanted % kFrameBytes != 0) {
        result.error = -1;
        return result;
    }
    std::array<std::uint8_t, kScratchBytes> scratch{};
    while (result.bytes < wanted) {
        const int ask = std::min(kScratchBytes, wanted - result.bytes);
        const int count = reader(scratch.data(), ask);
        if (count < 0) {
            result.error = count;
            break;
        }
        if (count == 0) {
            result.end_of_stream = true;
            break;
        }
        if (count > ask || count % kFrameBytes != 0) {
            result.error = -1;
            break;
        }
        std::memcpy(static_cast<std::uint8_t*>(output) + result.bytes,
                    scratch.data(), static_cast<std::size_t>(count));
        result.bytes += count;
    }
    return result;
}

} // namespace srhd_awa::platform::e2e4_vorbis_read

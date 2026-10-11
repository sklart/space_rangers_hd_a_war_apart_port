#include "e2e4_vorbis_read.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using srhd_awa::platform::e2e4_vorbis_read::ReadBlocks;

int main() {
    constexpr int wanted = 2 * 44100 * 4;
    std::array<std::uint8_t, wanted + 16> destination{};
    destination.fill(0xa5);
    int calls = 0;
    const auto full = ReadBlocks(destination.data() + 8, wanted,
        [&](std::uint8_t* scratch, int capacity) {
            assert(capacity > 0 && capacity <= 4096 && capacity % 4 == 0);
            ++calls;
            for (int i = 0; i < capacity; ++i) scratch[i] = static_cast<std::uint8_t>(i);
            return capacity;
        });
    assert(full.bytes == wanted && !full.end_of_stream && full.error == 0);
    assert(calls > 80 && destination[0] == 0xa5 &&
           destination[wanted + 8] == 0xa5);
    assert(destination[8] == 0 && destination[9] == 1 &&
           destination[8 + 4096] == 0);

    calls = 0;
    const auto short_read = ReadBlocks(destination.data() + 8, 8192,
        [&](std::uint8_t* scratch, int capacity) {
            ++calls;
            if (calls == 2) return 0;
            for (int i = 0; i < capacity; ++i) scratch[i] = 0x33;
            return capacity;
        });
    assert(short_read.bytes == 4096 && short_read.end_of_stream &&
           short_read.error == 0);

    const auto failure = ReadBlocks(destination.data() + 8, 8192,
        [](std::uint8_t*, int) { return -137; });
    assert(failure.bytes == 0 && failure.error == -137 &&
           !failure.end_of_stream);
    const auto overflow = ReadBlocks(destination.data() + 8, 4,
        [](std::uint8_t*, int capacity) { return capacity + 4; });
    assert(overflow.bytes == 0 && overflow.error != 0);
    const auto partial_frame = ReadBlocks(destination.data() + 8, 4,
        [](std::uint8_t*, int) { return 2; });
    assert(partial_frame.bytes == 0 && partial_frame.error != 0);
    assert(ReadBlocks(destination.data() + 8, -1,
        [](std::uint8_t*, int) { return 0; }).error != 0);
    assert(ReadBlocks(destination.data() + 8, 3,
        [](std::uint8_t*, int) { return 0; }).error != 0);
}

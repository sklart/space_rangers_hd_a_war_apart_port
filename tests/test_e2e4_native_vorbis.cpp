#include "e2e4_native_vorbis.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

using srhd_awa::platform::e2e4_native_vorbis::Close;
using srhd_awa::platform::e2e4_native_vorbis::Open;
using srhd_awa::platform::e2e4_native_vorbis::Read;

struct Source {
    std::vector<std::uint8_t> data;
    std::size_t position = 0;
    int calls = 0;
};

std::size_t ReadElements(void* output, std::size_t size,
                         std::size_t count, void* opaque) {
    auto& source = *static_cast<Source*>(opaque);
    ++source.calls;
    if (size == 0 || size > source.data.size() - source.position) return 0;
    const auto elements = std::min(count, (source.data.size() - source.position) / size);
    const auto bytes = elements * size;
    std::memcpy(output, source.data.data() + source.position, bytes);
    source.position += bytes;
    return elements;
}

std::size_t BadRead(void*, std::size_t, std::size_t count, void*) {
    return count + 1;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    assert(file.good());
    Source source;
    source.data.assign(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
    assert(!source.data.empty());
    const auto opened = Open(&source, &ReadElements);
    assert(opened && opened.error == 0);
    assert(opened.channels == 2 && opened.rate == 22050);
    assert(source.calls > 0);

    std::array<std::uint8_t, 4096 + 16> guarded{};
    guarded.fill(0xa5);
    std::size_t total = 0;
    bool heard_signal = false;
    bool reached_eof = false;
    for (int i = 0; i < 20; ++i) {
        const auto result = Read(opened.decoder, guarded.data() + 8, 4096);
        assert(result.error == 0 && result.bytes >= 0 && result.bytes <= 4096);
        assert(result.bytes % 4 == 0);
        assert(guarded[0] == 0xa5 && guarded[4096 + 8] == 0xa5);
        for (int j = 0; j < result.bytes; ++j)
            heard_signal |= guarded[8 + j] != 0;
        total += static_cast<std::size_t>(result.bytes);
        if (result.end_of_stream) {
            reached_eof = true;
            break;
        }
    }
    assert(reached_eof && heard_signal && total > 16000 && total < 40000);
    assert(Read(opened.decoder, guarded.data() + 8, 3).error != 0);
    Close(opened.decoder);

    source.position = 0;
    const auto reopened = Open(&source, &ReadElements);
    assert(reopened);
    Close(reopened.decoder);

    Source damaged;
    damaged.data.assign(source.data.begin(), source.data.begin() + 16);
    const auto invalid = Open(&damaged, &ReadElements);
    assert(!invalid && invalid.error != 0);
    const auto io_error = Open(&source, &BadRead);
    assert(!io_error && io_error.error != 0);
}

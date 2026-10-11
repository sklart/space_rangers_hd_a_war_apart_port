#pragma once

#include "e2e4_vorbis_read.hpp"

#include <cstddef>
#include <cstdint>

namespace srhd_awa::platform::e2e4_native_vorbis {

// Mirrors fread: returns a count of complete elements. The caller retains
// ownership of source and must keep it open until Close returns.
using ReadSource = std::size_t (*)(void* bytes, std::size_t size,
                                   std::size_t count, void* source);

struct Decoder;

struct OpenResult {
    Decoder* decoder = nullptr;
    int error = 0;
    int channels = 0;
    std::uint32_t rate = 0;
    explicit operator bool() const { return decoder != nullptr; }
};

OpenResult Open(void* source, ReadSource read);
e2e4_vorbis_read::Result Read(Decoder* decoder, void* pcm, int bytes);
void Close(Decoder* decoder) noexcept;

} // namespace srhd_awa::platform::e2e4_native_vorbis

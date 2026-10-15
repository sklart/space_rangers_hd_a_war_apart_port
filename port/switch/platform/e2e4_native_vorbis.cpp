#include "e2e4_native_vorbis.hpp"

#include <vorbis/vorbisfile.h>

#include <limits>
#include <memory>
#if defined(E2E5_HOST_SDL_WINDOW)
#include <atomic>
#include <cstdlib>
#include <new>
#endif

namespace srhd_awa::platform::e2e4_native_vorbis {

struct Decoder {
    OggVorbis_File file{};
    void* source = nullptr;
    ReadSource read = nullptr;
    int channels = 0;
    std::uint32_t rate = 0;
    bool opened = false;
    bool source_error = false;
};

namespace {

std::size_t ReadCallback(void* bytes, std::size_t size,
                         std::size_t count, void* userdata) noexcept {
    auto* decoder = static_cast<Decoder*>(userdata);
    if (!decoder || !decoder->read || !bytes || size == 0 || count == 0)
        return 0;
    try {
        const auto elements = decoder->read(bytes, size, count, decoder->source);
        if (elements <= count) return elements;
    } catch (...) {
    }
    decoder->source_error = true;
    return 0;
}

} // namespace

OpenResult Open(void* source, ReadSource read) {
#if defined(E2E5_HOST_SDL_WINDOW)
    // Isolated host fault injection exercises the original worker boundary.
    if (const char* value = std::getenv("E2E5_HOST_FAIL_VORBIS_OPEN")) {
        static std::atomic<long> calls{0};
        const auto target = std::strtol(value, nullptr, 10);
        if (target > 0 && calls.fetch_add(1) + 1 == target) throw std::bad_alloc();
    }
#endif
    OpenResult result;
    if (!source || !read) {
        result.error = OV_EINVAL;
        return result;
    }
    auto decoder = std::make_unique<Decoder>();
    decoder->source = source;
    decoder->read = read;
    ov_callbacks callbacks{};
    callbacks.read_func = &ReadCallback;
    const int opened = ov_open_callbacks(decoder.get(), &decoder->file,
                                         nullptr, 0, callbacks);
    if (opened != 0 || decoder->source_error) {
        result.error = decoder->source_error ? OV_EREAD : opened;
        if (opened == 0) ov_clear(&decoder->file);
        return result;
    }
    decoder->opened = true;
    const auto* info = ov_info(&decoder->file, -1);
    if (!info || (info->channels != 1 && info->channels != 2) ||
        info->rate <= 0 || info->rate > 192000) {
        result.error = OV_EBADHEADER;
        ov_clear(&decoder->file);
        return result;
    }
    decoder->channels = info->channels;
    decoder->rate = static_cast<std::uint32_t>(info->rate);
    result.channels = decoder->channels;
    result.rate = decoder->rate;
    result.decoder = decoder.release();
    return result;
}

e2e4_vorbis_read::Result Read(Decoder* decoder, void* pcm, int bytes) {
    if (!decoder || !decoder->opened) return {.error = OV_EINVAL};
    decoder->source_error = false;
    int bitstream = 0;
    auto result = e2e4_vorbis_read::ReadBlocks(pcm, bytes,
        [&](std::uint8_t* scratch, int capacity) {
            const long decoded = ov_read(&decoder->file,
                                         reinterpret_cast<char*>(scratch),
                                         capacity, 0, 2, 1, &bitstream);
            if (decoded < std::numeric_limits<int>::min() ||
                decoded > std::numeric_limits<int>::max()) return OV_EFAULT;
            return static_cast<int>(decoded);
        }, decoder->channels * 2);
    if (decoder->source_error) result.error = OV_EREAD;
    if (result.error == 0 && bitstream != 0) {
        const auto* info = ov_info(&decoder->file, bitstream);
        if (!info || info->channels != decoder->channels ||
            info->rate != decoder->rate) result.error = OV_EBADLINK;
    }
    return result;
}

void Close(Decoder* decoder) noexcept {
    if (!decoder) return;
    if (decoder->opened) ov_clear(&decoder->file);
    delete decoder;
}

} // namespace srhd_awa::platform::e2e4_native_vorbis

#include "zlib_bridge.hpp"

#include <array>
#include <limits>

#include <zlib.h>

namespace srhd_awa::platform::zlib_bridge {
namespace {
constexpr auto kMaxResult = static_cast<uLong>(std::numeric_limits<std::int32_t>::max());
}

std::int32_t Compress(void* destination, void* source, std::int32_t source_size,
                      std::int32_t mode) {
  if (!destination || !source || source_size <= 0) return 0;
  uLongf written = static_cast<uLong>(source_size);
  const int level = mode == 0 ? Z_BEST_SPEED : Z_DEFAULT_COMPRESSION;
  if (compress2(static_cast<Bytef*>(destination), &written,
                static_cast<const Bytef*>(source), static_cast<uLong>(source_size), level) != Z_OK ||
      written > kMaxResult) {
    return 0;
  }
  return static_cast<std::int32_t>(written);
}

std::int32_t Uncompress(void* destination, std::int32_t destination_capacity,
                        void* source, std::int32_t source_size) {
  if (!source || source_size <= 0) return 0;
  if (destination) {
    if (destination_capacity <= 0) return 0;
    uLongf written = static_cast<uLong>(destination_capacity);
    if (uncompress(static_cast<Bytef*>(destination), &written,
                   static_cast<const Bytef*>(source), static_cast<uLong>(source_size)) != Z_OK ||
        written > kMaxResult) {
      return 0;
    }
    return static_cast<std::int32_t>(written);
  }

  z_stream stream{};
  stream.next_in = static_cast<Bytef*>(source);
  stream.avail_in = static_cast<uInt>(source_size);
  if (inflateInit(&stream) != Z_OK) return 0;
  std::array<Bytef, 4096> chunk{};
  std::uint64_t total{};
  int result{};
  do {
    stream.next_out = chunk.data();
    stream.avail_out = static_cast<uInt>(chunk.size());
    result = inflate(&stream, Z_NO_FLUSH);
    total += chunk.size() - stream.avail_out;
    if (total > kMaxResult) {
      inflateEnd(&stream);
      return 0;
    }
  } while (result == Z_OK);
  inflateEnd(&stream);
  return result == Z_STREAM_END ? static_cast<std::int32_t>(total) : 0;
}

}  // namespace srhd_awa::platform::zlib_bridge

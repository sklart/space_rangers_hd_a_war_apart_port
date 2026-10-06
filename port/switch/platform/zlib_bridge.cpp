#include "zlib_bridge.hpp"

#include <array>
#include <limits>

#include <zlib.h>

namespace srhd_awa::platform::zlib_bridge {
namespace {
constexpr auto kMaxResult = static_cast<uLong>(std::numeric_limits<std::int32_t>::max());

struct CompressedInput {
  const Bytef* data{};
  uLong size{};
  std::int32_t expected_size{};
};

bool PrepareInput(void* source, std::int32_t source_size, CompressedInput* input) {
  if (!source || source_size <= 0) return false;
  const auto* bytes = static_cast<const Bytef*>(source);
  input->data = bytes;
  input->size = static_cast<uLong>(source_size);
  if (source_size < 4 || bytes[0] != 'Z' || bytes[1] != 'L' || bytes[2] != '0' || bytes[3] != '1')
    return true;
  if (source_size <= 8) return false;
  const auto expected = static_cast<std::uint32_t>(bytes[4]) |
                        (static_cast<std::uint32_t>(bytes[5]) << 8) |
                        (static_cast<std::uint32_t>(bytes[6]) << 16) |
                        (static_cast<std::uint32_t>(bytes[7]) << 24);
  if (expected == 0 || expected > kMaxResult) return false;
  input->data = bytes + 8;
  input->size = static_cast<uLong>(source_size - 8);
  input->expected_size = static_cast<std::int32_t>(expected);
  return true;
}
}  // namespace

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
  CompressedInput input;
  if (!PrepareInput(source, source_size, &input)) return 0;
  if (destination) {
    if (destination_capacity <= 0) return 0;
    uLongf written = static_cast<uLong>(destination_capacity);
    if (uncompress(static_cast<Bytef*>(destination), &written, input.data, input.size) != Z_OK ||
        written > kMaxResult ||
        (input.expected_size != 0 && written != static_cast<uLong>(input.expected_size))) {
      return 0;
    }
    return static_cast<std::int32_t>(written);
  }

  z_stream stream{};
  stream.next_in = const_cast<Bytef*>(input.data);
  stream.avail_in = static_cast<uInt>(input.size);
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
  if (result != Z_STREAM_END ||
      (input.expected_size != 0 && total != static_cast<std::uint64_t>(input.expected_size))) {
    return 0;
  }
  return static_cast<std::int32_t>(total);
}

}  // namespace srhd_awa::platform::zlib_bridge

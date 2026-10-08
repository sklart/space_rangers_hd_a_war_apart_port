#include "gi_format2_cpu.hpp"

#include "okgf_rle_bridge.hpp"
#include "rle_validation.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::gi_format2_cpu {
namespace {
constexpr std::uint64_t kMaxBytes = 256ull * 1024 * 1024;
constexpr std::size_t kHeaderBytes = 64;
constexpr std::size_t kPlaneBytes = 32;
constexpr std::size_t kRleHeaderBytes = 16;

struct Plane {
  bool present{};
  std::size_t offset{}, data_size{};
  std::int32_t origin_x{}, origin_y{}, width{}, height{};
  std::size_t stream_bytes{};
  const std::uint8_t* rle{};
};

std::uint32_t U32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::int32_t I32(const std::uint8_t* p) { return static_cast<std::int32_t>(U32(p)); }
bool Range(std::uint64_t offset, std::uint64_t size, std::size_t total) {
  return offset <= total && size <= static_cast<std::uint64_t>(total) - offset;
}
Status Fail(Status status, Metadata* metadata, CpuImage* image, std::string* error, const char* reason) {
  if (metadata) *metadata = {};
  if (image) image->Clear();
  if (error) *error = reason;
  return status;
}

Status ReadPlane(const std::uint8_t* data, std::size_t source_size, const Metadata& header,
                 std::int32_t index, Plane* out) {
  *out = {};
  const auto entry = kHeaderBytes + static_cast<std::size_t>(index) * kPlaneBytes;
  const auto offset = I32(data + entry);
  const auto data_size = I32(data + entry + 4);
  if (offset == 0) return Status::Ok;
  if (offset < 0 || data_size < static_cast<std::int32_t>(kRleHeaderBytes) ||
      !Range(static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(data_size), source_size))
    return Status::InvalidPlane;
  const auto left = I32(data + entry + 8), top = I32(data + entry + 12);
  const auto rle = data + static_cast<std::size_t>(offset);
  const auto stream_size = I32(rle), width = I32(rle + 4), height = I32(rle + 8);
  if (stream_size < 0 || width <= 0 || height <= 0 ||
      !Range(kRleHeaderBytes, static_cast<std::uint32_t>(stream_size), static_cast<std::size_t>(data_size)))
    return Status::InvalidRle;
  const std::int64_t origin_x = static_cast<std::int64_t>(left) - header.left;
  const std::int64_t origin_y = static_cast<std::int64_t>(top) - header.top;
  if (origin_x < 0 || origin_y < 0 || origin_x + width > header.width || origin_y + height > header.height)
    return Status::InvalidRle;
  const std::size_t literal_bytes = index == 2 ? 1 : 2;
  if (!rle_validation::ValidateStream(rle + kRleHeaderBytes, static_cast<std::size_t>(stream_size), width, height, literal_bytes))
    return Status::InvalidRle;
  out->present = true;
  out->offset = static_cast<std::size_t>(offset);
  out->data_size = static_cast<std::size_t>(data_size);
  out->origin_x = static_cast<std::int32_t>(origin_x);
  out->origin_y = static_cast<std::int32_t>(origin_y);
  out->width = width;
  out->height = height;
  out->stream_bytes = static_cast<std::size_t>(stream_size);
  out->rle = rle;
  return Status::Ok;
}
}  // namespace

Status Decode(const void* source, std::size_t source_size, Metadata* metadata, CpuImage* image,
              std::string* error) {
  if (metadata) *metadata = {};
  if (!image) return Fail(Status::InvalidHeader, metadata, image, error, "image");
  image->Clear();
  if (!source || source_size < kHeaderBytes) return Fail(Status::InvalidHeader, metadata, image, error, "header");
  const auto* data = static_cast<const std::uint8_t*>(source);
  if (data[0] != 'g' || data[1] != 'i') return Fail(Status::InvalidHeader, metadata, image, error, "magic");
  const auto version = I32(data + 4);
  if (version != 1) return Fail(Status::UnsupportedVersion, metadata, image, error, "version");
  const auto format = I32(data + 40);
  if (format != 2) return Fail(Status::UnsupportedFormat, metadata, image, error, "format");
  const auto left = I32(data + 8), top = I32(data + 12), right = I32(data + 16), bottom = I32(data + 20);
  const std::int64_t width = static_cast<std::int64_t>(right) - left;
  const std::int64_t height = static_cast<std::int64_t>(bottom) - top;
  if (width <= 0 || height <= 0 || width > std::numeric_limits<std::int32_t>::max() ||
      height > std::numeric_limits<std::int32_t>::max())
    return Fail(Status::InvalidBounds, metadata, image, error, "bounds");
  const auto plane_count = I32(data + 44), clip_count = I32(data + 48);
  if (plane_count < 3 || clip_count < 0 ||
      !Range(kHeaderBytes, static_cast<std::uint64_t>(plane_count) * kPlaneBytes, source_size))
    return Fail(Status::InvalidPlaneTable, metadata, image, error, "plane table");
  const auto pitch64 = width * 4;
  const auto bytes = pitch64 * height;
  if (pitch64 > std::numeric_limits<std::int32_t>::max() || static_cast<std::uint64_t>(bytes) > kMaxBytes)
    return Fail(Status::TooLarge, metadata, image, error, "limit");
  Metadata parsed{};
  parsed.version = version; parsed.left = left; parsed.top = top; parsed.right = right; parsed.bottom = bottom;
  parsed.width = static_cast<std::int32_t>(width); parsed.height = static_cast<std::int32_t>(height);
  parsed.plane_count = plane_count; parsed.clip_rect_count = clip_count;
  parsed.red_mask = U32(data + 24); parsed.green_mask = U32(data + 28);
  parsed.blue_mask = U32(data + 32); parsed.alpha_mask = U32(data + 36);
  Plane planes[3];
  for (std::int32_t index = 0; index < 3; ++index) {
    const auto status = ReadPlane(data, source_size, parsed, index, &planes[index]);
    if (status != Status::Ok) return Fail(status, metadata, image, error, status == Status::InvalidRle ? "RLE" : "plane");
  }
  image->width = parsed.width; image->height = parsed.height; image->pitch = static_cast<std::int32_t>(pitch64);
  image->bytes_per_pixel = 4; image->pixels.assign(static_cast<std::size_t>(bytes), 0);
  for (const std::int32_t index : {2, 1, 0}) {
    const auto& plane = planes[index];
    if (!plane.present) continue;
    auto* destination = image->pixels.data() + static_cast<std::size_t>(plane.origin_y) * image->pitch +
                        static_cast<std::size_t>(plane.origin_x) * 4;
    if (index == 2) okgf_rle_bridge::DrawAlphaBufRgba(destination, image->pitch, plane.rle);
    else if (index == 1) okgf_rle_bridge::DrawTransAlphaBufRgba(destination, image->pitch, plane.rle);
    else okgf_rle_bridge::DrawTransBufRgba(destination, image->pitch, plane.rle);
  }
  if (metadata) *metadata = parsed;
  return Status::Ok;
}

}  // namespace srhd_awa::platform::gi_format2_cpu

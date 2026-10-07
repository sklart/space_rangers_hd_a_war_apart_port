#include "gai_cpu.hpp"

#include "zlib_bridge.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace srhd_awa::platform::gai_cpu {
namespace {
#pragma pack(push, 1)
struct DiskGaiHeader { std::uint8_t magic[4]; std::int32_t version, left, top, right, bottom, frame_count; std::uint32_t flags; std::int32_t sequence_offset, sequence_size; std::uint8_t reserved[8]; };
struct DiskGaiFrameEntry { std::int32_t data_offset, data_size; };
struct DiskGaiSequenceTableHeader { std::int32_t sequence_count; std::uint8_t reserved[4]; };
struct DiskGaiSequenceDirectoryEntry { std::int32_t data_offset; std::uint8_t reserved[4]; };
struct DiskGaiSequenceDataBlock { std::int32_t frame_count; };
struct DiskGaiSequenceFrameEntry { std::int32_t source_frame_index, frame_delay; };
#pragma pack(pop)
static_assert(sizeof(DiskGaiHeader) == 48);
static_assert(sizeof(DiskGaiFrameEntry) == 8);
static_assert(sizeof(DiskGaiSequenceTableHeader) == 8);
static_assert(sizeof(DiskGaiSequenceDirectoryEntry) == 8);
static_assert(sizeof(DiskGaiSequenceDataBlock) == 4);
static_assert(sizeof(DiskGaiSequenceFrameEntry) == 8);
constexpr std::size_t kHeaderSize = 48;
constexpr std::size_t kFrameEntrySize = 8;
constexpr std::size_t kSequenceHeaderSize = 8;
constexpr std::size_t kSequenceDirectoryEntrySize = 8;
constexpr std::size_t kSequenceDataHeaderSize = 4;
constexpr std::size_t kSequenceFrameEntrySize = 8;
constexpr std::uint64_t kMaxFrames = 100000;
constexpr std::uint64_t kMaxDecodedBytes = 256ull * 1024 * 1024;

std::uint32_t U32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::int32_t I32(const std::uint8_t* p) { return static_cast<std::int32_t>(U32(p)); }
bool Range(std::uint64_t offset, std::uint64_t size, std::size_t total) {
  return offset <= total && size <= static_cast<std::uint64_t>(total) - offset;
}
Status Fail(Status status, GaiMetadata* metadata, GaiFrameInfo* info, GaiFramePayload* payload,
            gi_format0_cpu::Metadata* gi_metadata, gi_format0_cpu::CpuImage* image,
            std::string* error, const char* reason) {
  if (metadata) *metadata = {};
  if (info) *info = {};
  if (payload) payload->Clear();
  if (gi_metadata) *gi_metadata = {};
  if (image) image->Clear();
  if (error) *error = reason;
  return status;
}
FrameEncoding Encoding(const std::uint8_t* data, std::size_t size) {
  if (size < 2) return FrameEncoding::Unknown;
  if (data[0] == 'g' && data[1] == 'i') return FrameEncoding::RawGi;
  if (data[0] != 'Z' || data[1] != 'L') return FrameEncoding::Unknown;
  if (size < 4) return FrameEncoding::Unknown;
  if (data[2] == '0' && data[3] == '1') return FrameEncoding::Zl01;
  if (data[2] == '0' && data[3] == '2') return FrameEncoding::Zl02;
  return FrameEncoding::Unknown;
}
Status ValidateSequence(const std::uint8_t* data, std::size_t size, GaiMetadata* metadata,
                        std::string* error) {
  if (!metadata->sequence_table_offset) {
    if (metadata->sequence_table_size != 0)
      return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence size without offset");
    return Status::Ok;
  }
  if (metadata->sequence_table_offset < 0 || metadata->sequence_table_size < 0 ||
      !Range(static_cast<std::uint32_t>(metadata->sequence_table_offset),
             static_cast<std::uint32_t>(metadata->sequence_table_size), size)) {
    return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence range");
  }
  metadata->sequence_table_present = true;
  const auto offset = static_cast<std::size_t>(metadata->sequence_table_offset);
  const auto table_size = static_cast<std::size_t>(metadata->sequence_table_size);
  if (table_size < kSequenceHeaderSize) return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence header");
  const auto count = I32(data + offset);
  if (count < 0 || static_cast<std::uint64_t>(count) > kMaxFrames ||
      kSequenceHeaderSize + static_cast<std::uint64_t>(count) * kSequenceDirectoryEntrySize > table_size) {
    return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence directory");
  }
  metadata->sequence_count = count;
  for (std::int32_t i = 0; i < count; ++i) {
    const auto entry = offset + kSequenceHeaderSize + static_cast<std::size_t>(i) * kSequenceDirectoryEntrySize;
    const auto block_offset = I32(data + entry);
    if (block_offset < 0 || !Range(static_cast<std::uint32_t>(block_offset), kSequenceDataHeaderSize, table_size))
      return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence block");
    const auto block = offset + static_cast<std::size_t>(block_offset);
    const auto frames = I32(data + block);
    if (frames < 0 || static_cast<std::uint64_t>(frames) > kMaxFrames ||
        !Range(static_cast<std::uint32_t>(block_offset) + kSequenceDataHeaderSize,
               static_cast<std::uint64_t>(frames) * kSequenceFrameEntrySize, table_size))
      return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence frames");
    for (std::int32_t j = 0; j < frames; ++j) {
      const auto frame = block + kSequenceDataHeaderSize + static_cast<std::size_t>(j) * kSequenceFrameEntrySize;
      const auto source_index = I32(data + frame);
      if (source_index < 0 || source_index >= metadata->frame_count)
        return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "sequence source frame");
    }
  }
  return Status::Ok;
}
}  // namespace

const char* FrameEncodingName(FrameEncoding encoding) {
  switch (encoding) {
    case FrameEncoding::Empty: return "empty";
    case FrameEncoding::RawGi: return "raw";
    case FrameEncoding::Zl01: return "ZL01";
    case FrameEncoding::Zl02: return "ZL02";
    default: return "unknown";
  }
}

Status ValidateGai(const void* source, std::size_t source_size, GaiMetadata* metadata, std::string* error) {
  if (metadata) *metadata = {};
  if (error) error->clear();
  if (!source || source_size < kHeaderSize) return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "header");
  const auto* data = static_cast<const std::uint8_t*>(source);
  if (std::memcmp(data, "gai\0", 4) != 0) return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "magic");
  GaiMetadata parsed{};
  parsed.version = I32(data + 4);
  parsed.left = I32(data + 8); parsed.top = I32(data + 12); parsed.right = I32(data + 16); parsed.bottom = I32(data + 20);
  parsed.frame_count = I32(data + 24); parsed.flags = U32(data + 28);
  parsed.sequence_table_offset = I32(data + 32); parsed.sequence_table_size = I32(data + 36);
  if (parsed.version != 1) return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "version");
  if (parsed.right <= parsed.left || parsed.bottom <= parsed.top) return Fail(Status::InvalidHeader, metadata, nullptr, nullptr, nullptr, nullptr, error, "bounds");
  if (parsed.frame_count <= 0 || static_cast<std::uint64_t>(parsed.frame_count) > kMaxFrames ||
      !Range(kHeaderSize, static_cast<std::uint64_t>(parsed.frame_count) * kFrameEntrySize, source_size))
    return Fail(Status::InvalidDirectory, metadata, nullptr, nullptr, nullptr, nullptr, error, "frame directory");
  for (std::int32_t i = 0; i < parsed.frame_count; ++i) {
    const auto entry = kHeaderSize + static_cast<std::size_t>(i) * kFrameEntrySize;
    const auto offset = I32(data + entry), frame_size = I32(data + entry + 4);
    if (offset == 0) continue;
    if (offset < 0 || frame_size <= 0 || !Range(static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(frame_size), source_size))
      return Fail(Status::InvalidFrame, metadata, nullptr, nullptr, nullptr, nullptr, error, "frame range");
  }
  const auto status = ValidateSequence(data, source_size, &parsed, error);
  if (status != Status::Ok) return status;
  if (metadata) *metadata = parsed;
  return Status::Ok;
}

Status ReadGaiFrameInfo(const void* source, std::size_t source_size, std::int32_t index, GaiFrameInfo* info, std::string* error) {
  if (info) *info = {};
  GaiMetadata metadata{};
  auto status = ValidateGai(source, source_size, &metadata, error);
  if (status != Status::Ok) return status;
  if (!info || index < 0 || index >= metadata.frame_count) return Fail(Status::InvalidFrame, nullptr, info, nullptr, nullptr, nullptr, error, "frame index");
  const auto* data = static_cast<const std::uint8_t*>(source);
  const auto entry = kHeaderSize + static_cast<std::size_t>(index) * kFrameEntrySize;
  info->index = index; info->data_offset = I32(data + entry); info->data_size = I32(data + entry + 4);
  info->encoding = info->data_offset == 0 ? FrameEncoding::Empty : Encoding(data + static_cast<std::size_t>(info->data_offset), static_cast<std::size_t>(info->data_size));
  return Status::Ok;
}

Status ExtractGaiFrame(const void* source, std::size_t source_size, std::int32_t index, GaiFramePayload* payload, std::string* error) {
  if (payload) payload->Clear();
  GaiFrameInfo info{};
  auto status = ReadGaiFrameInfo(source, source_size, index, &info, error);
  if (status != Status::Ok) return Fail(status, nullptr, nullptr, payload, nullptr, nullptr, error, "frame");
  if (!payload) return Fail(Status::InvalidFrame, nullptr, nullptr, nullptr, nullptr, nullptr, error, "payload");
  payload->info = info;
  if (info.encoding == FrameEncoding::Empty) return Fail(Status::EmptyFrame, nullptr, nullptr, payload, nullptr, nullptr, error, "empty frame");
  if (info.encoding == FrameEncoding::Unknown) return Fail(Status::UnsupportedEncoding, nullptr, nullptr, payload, nullptr, nullptr, error, "encoding");
  const auto* frame = static_cast<const std::uint8_t*>(source) + static_cast<std::size_t>(info.data_offset);
  const auto frame_size = static_cast<std::size_t>(info.data_size);
  if (info.encoding == FrameEncoding::RawGi) { payload->gi_bytes.assign(frame, frame + frame_size); return Status::Ok; }
  if (frame_size < 8) return Fail(Status::DecompressionFailed, nullptr, nullptr, payload, nullptr, nullptr, error, "compressed header");
  const auto expected = U32(frame + 4);
  if (!expected || expected > kMaxDecodedBytes || expected > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
    return Fail(Status::DecompressionFailed, nullptr, nullptr, payload, nullptr, nullptr, error, "decoded limit");
  payload->gi_bytes.resize(expected);
  std::int32_t actual{};
  if (info.encoding == FrameEncoding::Zl01)
    actual = zlib_bridge::Uncompress(payload->gi_bytes.data(), static_cast<std::int32_t>(expected), const_cast<std::uint8_t*>(frame), static_cast<std::int32_t>(frame_size));
  else
    actual = zlib_bridge::UncompressZl02(payload->gi_bytes.data(), static_cast<std::int32_t>(expected), const_cast<std::uint8_t*>(frame), static_cast<std::int32_t>(frame_size));
  if (actual <= 0 || static_cast<std::uint32_t>(actual) != expected)
    return Fail(Status::DecompressionFailed, nullptr, nullptr, payload, nullptr, nullptr, error, "inflate");
  return Status::Ok;
}

Status DecodeGaiFormat0Frame(const void* source, std::size_t source_size, std::int32_t index,
                             GaiMetadata* gai_metadata, GaiFrameInfo* frame_info,
                             gi_format0_cpu::Metadata* gi_metadata, gi_format0_cpu::CpuImage* image, std::string* error) {
  if (gai_metadata) *gai_metadata = {};
  if (frame_info) *frame_info = {};
  if (gi_metadata) *gi_metadata = {};
  if (image) image->Clear();
  GaiMetadata metadata{}; auto status = ValidateGai(source, source_size, &metadata, error);
  if (status != Status::Ok) return status;
  GaiFramePayload payload{};
  status = ExtractGaiFrame(source, source_size, index, &payload, error);
  if (status != Status::Ok) return status;
  const auto gi_status = gi_format0_cpu::Decode(payload.gi_bytes.data(), payload.gi_bytes.size(), gi_metadata, image, error);
  if (gi_status != gi_format0_cpu::Status::Ok) {
    if (gi_metadata) *gi_metadata = {};
    if (image) image->Clear();
    return Status::GiDecodeFailed;
  }
  if (gai_metadata) *gai_metadata = metadata;
  if (frame_info) *frame_info = payload.info;
  return Status::Ok;
}

}  // namespace srhd_awa::platform::gai_cpu

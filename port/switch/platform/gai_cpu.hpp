#pragma once

#include "gi_format0_cpu.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace srhd_awa::platform::gai_cpu {

enum class Status {
  Ok,
  InvalidHeader,
  InvalidDirectory,
  InvalidFrame,
  EmptyFrame,
  UnsupportedEncoding,
  DecompressionFailed,
  GiDecodeFailed,
};

enum class FrameEncoding { Empty, RawGi, Zl01, Zl02, Unknown };

struct GaiMetadata {
  std::int32_t version{};
  std::int32_t left{}, top{}, right{}, bottom{};
  std::int32_t frame_count{};
  std::uint32_t flags{};
  std::int32_t sequence_table_offset{}, sequence_table_size{}, sequence_count{};
  bool sequence_table_present{};
};

struct GaiFrameInfo {
  std::int32_t index{};
  std::int32_t data_offset{}, data_size{};
  FrameEncoding encoding{FrameEncoding::Empty};
};

struct GaiFramePayload {
  GaiFrameInfo info{};
  std::vector<std::uint8_t> gi_bytes;
  void Clear() { info = {}; gi_bytes.clear(); }
};

Status ValidateGai(const void* source, std::size_t source_size, GaiMetadata* metadata,
                   std::string* error = nullptr);
Status ReadGaiFrameInfo(const void* source, std::size_t source_size, std::int32_t index,
                        GaiFrameInfo* info, std::string* error = nullptr);
Status ExtractGaiFrame(const void* source, std::size_t source_size, std::int32_t index,
                       GaiFramePayload* payload, std::string* error = nullptr);
Status DecodeGaiFormat0Frame(const void* source, std::size_t source_size, std::int32_t index,
                             GaiMetadata* gai_metadata, GaiFrameInfo* frame_info,
                             gi_format0_cpu::Metadata* gi_metadata,
                             gi_format0_cpu::CpuImage* image, std::string* error = nullptr);

const char* FrameEncodingName(FrameEncoding encoding);

}  // namespace srhd_awa::platform::gai_cpu

#pragma once

#include "gi_format0_cpu.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace srhd_awa::platform::gi_format2_cpu {

enum class Status {
  Ok,
  InvalidHeader,
  UnsupportedVersion,
  UnsupportedFormat,
  InvalidBounds,
  InvalidPlaneTable,
  InvalidPlane,
  InvalidRle,
  TooLarge,
};

using CpuImage = gi_format0_cpu::CpuImage;

struct Metadata {
  std::int32_t version{}, left{}, top{}, right{}, bottom{}, width{}, height{};
  std::int32_t plane_count{}, clip_rect_count{};
  std::uint32_t red_mask{}, green_mask{}, blue_mask{}, alpha_mask{};
};

Status Decode(const void* source, std::size_t source_size, Metadata* metadata, CpuImage* image,
              std::string* error = nullptr);

}  // namespace srhd_awa::platform::gi_format2_cpu

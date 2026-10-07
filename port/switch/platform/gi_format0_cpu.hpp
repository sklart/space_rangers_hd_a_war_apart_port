#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace srhd_awa::platform::gi_format0_cpu {
enum class Status { Ok, InvalidHeader, UnsupportedVersion, UnsupportedFormat, InvalidBounds, InvalidPlaneTable, InvalidPlane, PayloadTooSmall, TooLarge };
struct Metadata { std::int32_t version{},left{},top{},right{},bottom{},width{},height{},plane_count{},clip_rect_count{}; std::uint32_t red_mask{},green_mask{},blue_mask{},alpha_mask{}; };
struct CpuImage { std::int32_t width{},height{},pitch{},bytes_per_pixel{}; std::vector<std::uint8_t> pixels; void Clear(){width=height=pitch=bytes_per_pixel=0;pixels.clear();} };
Status Decode(const void*,std::size_t,Metadata*,CpuImage*,std::string* = nullptr);
}

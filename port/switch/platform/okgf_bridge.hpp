#pragma once
#include <cstdint>
#include "types/Windows_group.hpp"
namespace srhd_awa::platform::okgf_bridge {
void FillWord(void*, std::int32_t, std::int32_t, std::int32_t, std::uint16_t);
void PixelAlpha16(void*, std::uint16_t, std::uint8_t);
void LineIp16(void*, std::int32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t);
void Triangle16(void*, std::int32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t, const WindowsSdk::TRect*);
}

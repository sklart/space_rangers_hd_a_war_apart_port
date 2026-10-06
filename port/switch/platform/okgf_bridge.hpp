#pragma once
#include <cstdint>
#include "types/Windows_group.hpp"
namespace srhd_awa::platform::okgf_bridge {
void CopyWord(void*, std::int32_t, std::int32_t, std::int32_t, void*, std::int32_t,
              std::int32_t, std::int32_t, std::int32_t, std::int32_t);
void FillWord(void*, std::int32_t, std::int32_t, std::int32_t, std::uint16_t);
void Convert565ToBgra(void*, std::int32_t, void*, std::int32_t, std::int32_t, std::int32_t);
void PixelAlpha16(void*, std::uint16_t, std::uint8_t);
void* BeginImageRead(void*, std::int32_t, std::int32_t*, std::int32_t*);
std::int32_t ReadImagePixels(void*, void*, std::int32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t);
void LineIp16(void*, std::int32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t);
void Triangle16(void*, std::int32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t, const WindowsSdk::TRect*);
}

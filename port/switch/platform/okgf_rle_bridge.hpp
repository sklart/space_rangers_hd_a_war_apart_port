#pragma once

#include <cstdint>

namespace srhd_awa::platform::okgf_rle_bridge {

void DrawAlphaBufRgba(void* destination, std::int32_t pitch, const void* source);
void DrawTransAlphaBufRgba(void* destination, std::int32_t pitch, const void* source);
void DrawTransBufRgba(void* destination, std::int32_t pitch, const void* source);

}  // namespace srhd_awa::platform::okgf_rle_bridge

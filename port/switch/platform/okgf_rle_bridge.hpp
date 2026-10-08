#pragma once

#include <cstdint>

namespace srhd_awa::platform::okgf_rle_bridge {

struct Rect { std::int32_t left{}, top{}, right{}, bottom{}; };

std::int32_t BuildTransBufFromBgra(const void* source, std::int32_t pitch,
                                   std::int32_t width, std::int32_t height, void* destination);
std::int32_t BuildTransAlphaBufFromBgra(const void* source, std::int32_t pitch,
                                        std::int32_t width, std::int32_t height, void* destination);
std::int32_t BuildAlphaBufFromBgra(const void* source, std::int32_t pitch,
                                   std::int32_t width, std::int32_t height, void* destination);
std::int32_t BuildTransBufWord(const void* source, std::int32_t pitch,
                               std::int32_t width, std::int32_t height, void* destination,
                               std::uint16_t transparent);

void DrawAlphaBufRgba(void* destination, std::int32_t pitch, const void* source);
void DrawTransAlphaBufRgba(void* destination, std::int32_t pitch, const void* source);
void DrawTransBufRgba(void* destination, std::int32_t pitch, const void* source);
void DrawAlphaBuf565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                         const void* source, const Rect& clip);
void DrawTransAlphaBuf565Clip(void* destination, std::int32_t pitch, std::int32_t x,
                              std::int32_t y, const void* source, const Rect& clip);
void DrawTransBuf565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                         const void* source, const Rect& clip);
void DrawTransBufHalf565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                             const void* source, const Rect& clip);

}  // namespace srhd_awa::platform::okgf_rle_bridge

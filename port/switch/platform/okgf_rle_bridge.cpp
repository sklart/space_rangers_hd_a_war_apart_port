#include "okgf_rle_bridge.hpp"

#include <okgf.h>

namespace srhd_awa::platform::okgf_rle_bridge {

namespace {
OkgfRect ToOkgfRect(const Rect& rect) { return {rect.left, rect.top, rect.right - 1, rect.bottom - 1}; }
}

std::int32_t BuildTransBufFromBgra(const void* source, std::int32_t pitch,
                                   std::int32_t width, std::int32_t height, void* destination) {
  return OKGR_TransBuf_BuildFromRGBA_16(source, pitch, width, height, destination);
}
std::int32_t BuildTransAlphaBufFromBgra(const void* source, std::int32_t pitch,
                                        std::int32_t width, std::int32_t height, void* destination) {
  return OKGR_TransAlphaBuf_BuildFromRGBA_16(source, pitch, width, height, destination);
}
std::int32_t BuildAlphaBufFromBgra(const void* source, std::int32_t pitch,
                                   std::int32_t width, std::int32_t height, void* destination) {
  return OKGR_AlphaBuf_BuildFromRGBA(source, pitch, width, height, destination);
}
std::int32_t BuildTransBufWord(const void* source, std::int32_t pitch,
                               std::int32_t width, std::int32_t height, void* destination,
                               std::uint16_t transparent) {
  return OKGR_TransBuf_Build_WORD(source, pitch, width, height, destination, transparent);
}

void DrawAlphaBufRgba(void* destination, std::int32_t pitch, const void* source) {
  OKGR_AlphaBuf_Draw_RGBA(destination, pitch, static_cast<const OkgfRleHeader*>(source));
}
void DrawTransAlphaBufRgba(void* destination, std::int32_t pitch, const void* source) {
  OKGR_TransAlphaBuf_Draw_RGBA(destination, pitch, static_cast<const OkgfRleHeader*>(source));
}
void DrawTransBufRgba(void* destination, std::int32_t pitch, const void* source) {
  OKGR_TransBuf_Draw_RGBA(destination, pitch, static_cast<const OkgfRleHeader*>(source));
}
void DrawAlphaBuf565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                         const void* source, const Rect& clip) {
  const auto native = ToOkgfRect(clip);
  OKGR_AlphaBuf_DrawClip_16(destination, pitch, x, y,
                             static_cast<const OkgfRleHeader*>(source), &native);
}
void DrawTransAlphaBuf565Clip(void* destination, std::int32_t pitch, std::int32_t x,
                              std::int32_t y, const void* source, const Rect& clip) {
  const auto native = ToOkgfRect(clip);
  OKGR_TransAlphaBuf_DrawClip_WORD(destination, pitch, x, y,
                                   static_cast<const OkgfRleHeader*>(source), &native);
}
void DrawTransBuf565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                         const void* source, const Rect& clip) {
  const auto native = ToOkgfRect(clip);
  OKGR_TransBuf_DrawClip_WORD(destination, pitch, x, y,
                               static_cast<const OkgfRleHeader*>(source), &native);
}
void DrawTransBufHalf565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                             const void* source, const Rect& clip) {
  const auto native = ToOkgfRect(clip);
  OKGR_TransBuf_HADrawClip_16(destination, pitch, x, y,
                               static_cast<const OkgfRleHeader*>(source), &native);
}
void DrawMask565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                     const void* source, std::uint16_t color, const Rect& clip) {
  const auto native = ToOkgfRect(clip);
  OKGR_MaskBuf_DrawClip_WORD(destination, pitch, x, y,
                             static_cast<const OkgfRleHeader*>(source), color, &native);
}
void FillAlpha565Clip(void* destination, std::int32_t pitch, std::int32_t x, std::int32_t y,
                      const void* source, std::uint16_t color, const Rect& clip) {
  const auto native = ToOkgfRect(clip);
  OKGR_TransBuf_FillAlphaClip_16(destination, pitch, x, y,
                                 static_cast<const OkgfRleHeader*>(source), &native, color);
}

}  // namespace srhd_awa::platform::okgf_rle_bridge

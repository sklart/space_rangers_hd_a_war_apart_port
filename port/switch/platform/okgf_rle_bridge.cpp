#include "okgf_rle_bridge.hpp"

#include <okgf.h>

namespace srhd_awa::platform::okgf_rle_bridge {

void DrawAlphaBufRgba(void* destination, std::int32_t pitch, const void* source) {
  OKGR_AlphaBuf_Draw_RGBA(destination, pitch, static_cast<const OkgfRleHeader*>(source));
}
void DrawTransAlphaBufRgba(void* destination, std::int32_t pitch, const void* source) {
  OKGR_TransAlphaBuf_Draw_RGBA(destination, pitch, static_cast<const OkgfRleHeader*>(source));
}
void DrawTransBufRgba(void* destination, std::int32_t pitch, const void* source) {
  OKGR_TransBuf_Draw_RGBA(destination, pitch, static_cast<const OkgfRleHeader*>(source));
}

}  // namespace srhd_awa::platform::okgf_rle_bridge

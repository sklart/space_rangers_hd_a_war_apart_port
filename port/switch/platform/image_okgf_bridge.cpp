#include "image_okgf_bridge.hpp"

#include <okgf.h>

namespace srhd_awa::platform::image_okgf_bridge {
namespace {
constexpr std::uint64_t kMaxDecodedBytes = 256ull * 1024ull * 1024ull;
}

void* BeginRead(const void* source, std::int32_t size, std::int32_t* width,
                std::int32_t* height) {
  if (!source || size <= 0 || !width || !height) return nullptr;
  *width = 0;
  *height = 0;
  std::int32_t decoded_width{};
  std::int32_t decoded_height{};
  auto* context = ::OKGF_ReadStart_Buf(source, size, &decoded_width, &decoded_height);
  if (!context) return nullptr;
  const bool invalid = decoded_width <= 0 || decoded_height <= 0 ||
      static_cast<std::uint64_t>(decoded_width) > kMaxDecodedBytes / 4 ||
      static_cast<std::uint64_t>(decoded_height) >
          kMaxDecodedBytes / (static_cast<std::uint64_t>(decoded_width) * 4);
  if (invalid) {
    ::okgf_cancel_read(context);
    return nullptr;
  }
  *width = decoded_width;
  *height = decoded_height;
  return context;
}

void CancelRead(void* context) {
  if (context) ::okgf_cancel_read(static_cast<OkgfReadContext*>(context));
}

bool Read(void* context, void* pixels, std::int32_t pitch, std::uint32_t red,
          std::uint32_t green, std::uint32_t blue, std::uint32_t alpha,
          std::int32_t bytes_per_pixel) {
  return context && pixels && ::OKGF_Read(static_cast<OkgfReadContext*>(context), pixels, pitch,
                                           red, green, blue, alpha, bytes_per_pixel) != 0;
}

}  // namespace srhd_awa::platform::image_okgf_bridge

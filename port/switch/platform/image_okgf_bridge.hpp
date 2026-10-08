#pragma once

#include <cstdint>

namespace srhd_awa::platform::image_okgf_bridge {

void* BeginRead(const void* source, std::int32_t size, std::int32_t* width,
                std::int32_t* height);
void CancelRead(void* context);
bool Read(void* context, void* pixels, std::int32_t pitch, std::uint32_t red,
          std::uint32_t green, std::uint32_t blue, std::uint32_t alpha,
          std::int32_t bytes_per_pixel);

}  // namespace srhd_awa::platform::image_okgf_bridge

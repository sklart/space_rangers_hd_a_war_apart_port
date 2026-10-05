#pragma once

#include <cstdint>

namespace srhd_awa::platform::zlib_bridge {

std::int32_t Compress(void* destination, void* source, std::int32_t source_size,
                      std::int32_t mode);
std::int32_t Uncompress(void* destination, std::int32_t destination_capacity,
                        void* source, std::int32_t source_size);

}  // namespace srhd_awa::platform::zlib_bridge

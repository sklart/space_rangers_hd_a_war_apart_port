#include "gi_format0_okgf_bridge.hpp"

#include <okgf.h>

namespace srhd_awa::platform::gi_format0_okgf_bridge {

void Convert565ToBgra(void* source, std::int32_t source_pitch, void* destination,
                      std::int32_t destination_pitch, std::int32_t width, std::int32_t height) {
  ::OKGF_Convert565toBGRA(source, source_pitch, destination, destination_pitch, width, height);
}

}  // namespace srhd_awa::platform::gi_format0_okgf_bridge

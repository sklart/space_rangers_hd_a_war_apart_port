#pragma once

#include <cstdint>

namespace srhd_awa::platform::e2e4_ui_cache {

// UI cache entries refer to live blocks. Their search key must retain every
// address bit on ARM64; the serialized script dword format is unrelated.
inline std::uintptr_t Key(const void* block) noexcept {
    return reinterpret_cast<std::uintptr_t>(block);
}

} // namespace srhd_awa::platform::e2e4_ui_cache

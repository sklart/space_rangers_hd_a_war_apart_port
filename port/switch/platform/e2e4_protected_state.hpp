#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::e2e4_protected_state {

// The original algorithm XORs the byte ranges between selected live pointer
// fields. The C++ object header and excluded fields have native pointer width.
template <class XorBytes>
bool TransformObject(void* object, std::size_t object_bytes,
                     const std::uintptr_t* exclusions, std::size_t count,
                     XorBytes&& xor_bytes) {
    if (object == nullptr || object_bytes < sizeof(void*) ||
        object_bytes > std::numeric_limits<std::uint32_t>::max() ||
        (count != 0 && exclusions == nullptr)) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(object);
    if (object_bytes > std::numeric_limits<std::uintptr_t>::max() - base)
        return false;
    const auto limit = base + object_bytes;
    auto cursor = base + sizeof(void*);
    for (std::size_t i = 0; i < count; ++i) {
        const auto excluded = exclusions[i];
        if (excluded < cursor || excluded > limit ||
            limit - excluded < sizeof(void*)) return false;
        cursor = excluded + sizeof(void*);
    }
    cursor = base + sizeof(void*);
    for (std::size_t i = 0; i < count; ++i) {
        const auto excluded = exclusions[i];
        xor_bytes(reinterpret_cast<std::uint8_t*>(cursor), excluded - cursor);
        cursor = excluded + sizeof(void*);
    }
    xor_bytes(reinterpret_cast<std::uint8_t*>(cursor), limit - cursor);
    return true;
}

} // namespace srhd_awa::platform::e2e4_protected_state

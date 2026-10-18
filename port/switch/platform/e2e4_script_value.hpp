#pragma once

#include <bit>
#include <cstdint>
#include <limits>

namespace srhd_awa::platform::e2e4_script_value {

// Int storage can carry an address; numeric Int operations retain the original
// signed 32-bit value. Converting through uint32_t defines the low-bit view
// explicitly, without changing pointer storage. The int64_t input also accepts
// System::Trunc before narrowing on a 32-bit host.
inline constexpr std::int32_t NumericIntFromStorage(std::int64_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

// Win32 script DWORDs used signed int cells as bit containers. Preserve that
// conversion for numeric values while retaining a native address above 4 GiB.
inline constexpr std::intptr_t IntStorageFromDword(std::uintptr_t value) {
    if (value <= std::numeric_limits<std::uint32_t>::max()) {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }
    return std::bit_cast<std::intptr_t>(value);
}

inline constexpr std::uintptr_t DwordFromIntStorage(std::intptr_t value) {
    if (value >= std::numeric_limits<std::int32_t>::min() &&
        value <= std::numeric_limits<std::int32_t>::max()) {
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    }
    return std::bit_cast<std::uintptr_t>(value);
}

inline constexpr bool FitsDiskDword(std::uintptr_t value) {
    return value <= std::numeric_limits<std::uint32_t>::max();
}

inline constexpr bool FitsDiskInt(std::intptr_t value) {
    return value >= std::numeric_limits<std::int32_t>::min() &&
           value <= std::numeric_limits<std::int32_t>::max();
}

}  // namespace srhd_awa::platform::e2e4_script_value

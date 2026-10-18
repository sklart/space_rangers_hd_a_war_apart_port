#include "e2e4_script_value.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

namespace value = srhd_awa::platform::e2e4_script_value;

int main() {
    static_assert(value::NumericIntFromStorage(-123) == -123);
    static_assert(value::NumericIntFromStorage(0x7fffffff) == 0x7fffffff);
    static_assert(value::NumericIntFromStorage(4294967295LL) == -1);
    static_assert(value::DwordFromIntStorage(-1) == 0xffffffffu);
    static_assert(value::IntStorageFromDword(0xffffffffu) == -1);
    static_assert(value::IntStorageFromDword(0x80000000u) ==
                  std::numeric_limits<std::int32_t>::min());
    static_assert(value::DwordFromIntStorage(std::numeric_limits<std::int32_t>::min()) ==
                  0x80000000u);
    static_assert(value::DwordFromIntStorage(42) == 42);
    static_assert(value::FitsDiskDword(0xffffffffu));

    if constexpr (sizeof(std::uintptr_t) > sizeof(std::uint32_t)) {
        assert(value::NumericIntFromStorage(static_cast<std::intptr_t>(0x12345678ffffffffULL)) == -1);
        assert(value::NumericIntFromStorage(static_cast<std::intptr_t>(0x1234567880000000ULL)) ==
               std::numeric_limits<std::int32_t>::min());
        assert(!value::FitsDiskInt(static_cast<std::intptr_t>(0x80000000u)));
        const std::uintptr_t high = (std::uintptr_t{1} << 40) | 0x12345678u;
        const std::uintptr_t collision = high & 0xffffffffu;
        assert(high != collision);
        assert(value::DwordFromIntStorage(value::IntStorageFromDword(high)) == high);
        assert(value::DwordFromIntStorage(value::IntStorageFromDword(collision)) == collision);
        assert(!value::FitsDiskDword(high));
        assert(!value::FitsDiskInt(value::IntStorageFromDword(high)));
    }

    const std::uintptr_t numeric = std::numeric_limits<std::uint32_t>::max();
    assert(value::DwordFromIntStorage(value::IntStorageFromDword(numeric)) == numeric);
}

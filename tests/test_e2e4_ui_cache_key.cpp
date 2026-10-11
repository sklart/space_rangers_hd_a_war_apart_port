#include "e2e4_ui_cache_key.hpp"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    using srhd_awa::platform::e2e4_ui_cache::Key;
    static_assert(sizeof(std::uintptr_t) >= 8,
                  "This regression requires a 64-bit host");
    const std::array<std::uintptr_t, 4> addresses = {
        0x0000000100001234ULL,
        0x0000000200001234ULL,
        0x0000000300001234ULL,
        0x0000000400001234ULL,
    };
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        const auto* block = reinterpret_cast<const void*>(addresses[i]);
        assert(Key(block) == addresses[i]);
        for (std::size_t j = i + 1; j < addresses.size(); ++j) {
            assert(Key(block) < Key(reinterpret_cast<const void*>(addresses[j])));
        }
    }
}

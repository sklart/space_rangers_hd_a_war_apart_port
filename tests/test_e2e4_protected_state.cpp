#include "e2e4_protected_state.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

using srhd_awa::platform::e2e4_protected_state::TransformObject;

struct Sample {
    virtual ~Sample() = default;
    std::uint32_t credits = 0x12345678;
    void* inventory = nullptr;
    std::uint64_t amount = 0xabcdef1234567890ULL;
    void* quests = nullptr;
    std::uint32_t turn = 98765;
};

static void Apply(Sample& object) {
    const std::array<std::uintptr_t, 2> exclusions{
        reinterpret_cast<std::uintptr_t>(&object.inventory),
        reinterpret_cast<std::uintptr_t>(&object.quests)};
    std::uint32_t seed = 0x1b46a51u;
    const bool okay = TransformObject(&object, sizeof(object), exclusions.data(),
        exclusions.size(), [&](std::uint8_t* bytes, std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                seed = seed * 1664525u + 1013904223u;
                bytes[i] ^= static_cast<std::uint8_t>(seed >> 24);
            }
        });
    assert(okay);
}

int main() {
    Sample object;
    int inventory = 1;
    int quests = 2;
    object.inventory = &inventory;
    object.quests = &quests;
    std::array<std::uint8_t, sizeof(Sample)> original{};
    std::memcpy(original.data(), &object, sizeof(object));
    Apply(object);
    assert(object.inventory == &inventory && object.quests == &quests);
    assert(&object.inventory != &object.quests);
    Apply(object);
    assert(std::memcmp(original.data(), &object, sizeof(object)) == 0);
    assert(*static_cast<int*>(object.inventory) == 1);
    assert(*static_cast<int*>(object.quests) == 2);
    const std::array<std::uintptr_t, 2> reversed{
        reinterpret_cast<std::uintptr_t>(&object.quests),
        reinterpret_cast<std::uintptr_t>(&object.inventory)};
    assert(!TransformObject(&object, sizeof(object), reversed.data(),
        reversed.size(), [](std::uint8_t*, std::size_t) {}));
    assert(!TransformObject(nullptr, sizeof(object), nullptr, 0,
        [](std::uint8_t*, std::size_t) {}));
}

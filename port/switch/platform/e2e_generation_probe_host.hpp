#pragma once

// Isolated host diagnostics only. Production RNG, saves and Switch are unchanged.
#if defined(E2E5_HOST_SDL_WINDOW)
#include "runtime_support.hpp"
#include "types/EC_Buf.hpp"
#include "units/EC_Buf.hpp"
#include "types/aGalaxy.hpp"
#include "types/aPlanet.hpp"
#include "types/aShip.hpp"
#include "units/System.hpp"
#include "e2e_stage.hpp"
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace srhd_awa::platform::e2e_generation_probe_host {
inline std::chrono::steady_clock::time_point began;
inline std::uint32_t Seed() {
    const char* text = std::getenv("E2E511_HOST_GENERATION_SEED");
    if (!text) return 0;
    if (!*text) throw std::runtime_error("empty host generation seed");
    std::uint64_t value = 0;
    for (const char* p = text; *p; ++p) {
        if (*p < '0' || *p > '9') throw std::runtime_error("invalid host generation seed");
        value = value * 10 + (*p - '0');
        if (value > INT32_MAX) throw std::runtime_error("host generation seed out of range");
    }
    if (!value) throw std::runtime_error("host generation seed must be positive");
    return static_cast<std::uint32_t>(value);
}
inline bool SetSystemInput() {
    const auto seed = Seed();
    if (!seed) return false;
    began = std::chrono::steady_clock::now();
    System::RandSeed = seed;
    return true;
}
inline void SetGalaxyInput(aGalaxy::TGalaxy* galaxy) {
    const auto seed = Seed();
    if (!seed) return;
    // Match the native custom-seed assignment. The separate RTL input seed
    // replaces only Randomize() in this fixture; no RNG draw is removed.
    galaxy->GenerationSeed = seed;
    galaxy->RandomState = seed;
    char detail[160]{};
    std::snprintf(detail, sizeof(detail),
        "HOST generation fixture seed=%u rtl_state=%u chaotic=%u", seed,
        System::RandSeed, static_cast<unsigned>(galaxy->CustomRules.ChaoticRandom));
    e2e_stage::Log(detail);
}
struct Hash {
    std::uint64_t value = UINT64_C(14695981039346656037);
    void Word(std::uint32_t v) {
        for (unsigned i = 0; i < 4; ++i) { value ^= (v >> (i * 8)) & 255; value *= UINT64_C(1099511628211); }
    }
    void Float(float v) { Word(std::bit_cast<std::uint32_t>(v)); }
    void Text(const pas::WideString& s) {
        Word(s.length());
        for (int i = 0; i < s.length(); ++i) Word(s.c_str()[i]);
    }
};
inline void Checkpoint(aGalaxy::TGalaxy* g, int warmup_turn, bool final = false) {
    if (!Seed()) return;
    if (!final && warmup_turn != 0 && warmup_turn % 25 != 0) return;
    Hash stars, planets, ships;
    std::uint32_t ship_count = 0;
    for (int i = 0; i < pas::list_count(g->Stars); ++i) {
        const auto* star = pas::list_at<aGalaxy::TStar>(g->Stars, i);
        stars.Word(star->Id); stars.Word(star->GenerationSeed); stars.Word(star->RandomState);
        stars.Float(star->Position.X); stars.Float(star->Position.Y); stars.Text(star->Name);
        for (int j = 0; j < pas::list_count(star->Ships); ++j) {
            const auto* ship = pas::list_at<aShip::TShip>(star->Ships, j);
            ++ship_count;
            ships.Word(star->Id); ships.Word(ship->Id); ships.Word(ship->RandomState);
            ships.Word(ship->Seed); ships.Word(ship->Money); ships.Word(ship->OwnerId);
            ships.Float(ship->Position.X); ships.Float(ship->Position.Y); ships.Text(ship->Name);
        }
    }
    for (int i = 0; i < pas::list_count(g->ShipsInTransit); ++i) {
        const auto* ship = pas::list_at<aShip::TShip>(g->ShipsInTransit, i);
        ++ship_count; ships.Word(0); ships.Word(ship->Id); ships.Word(ship->RandomState);
        ships.Word(ship->Seed); ships.Word(ship->Money); ships.Word(ship->OwnerId);
        ships.Float(ship->Position.X); ships.Float(ship->Position.Y); ships.Text(ship->Name);
    }
    for (int i = 0; i < pas::list_count(g->Planets); ++i) {
        const auto* planet = pas::list_at<aPlanet::TPlanet>(g->Planets, i);
        planets.Word(planet->Id); planets.Word(planet->GenerationSeed); planets.Word(planet->RandomState);
        planets.Word(planet->Population); planets.Word(planet->Money); planets.Word(planet->OwnerId);
        planets.Text(planet->Name);
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - began).count();
    FILE* file = std::fopen("generation-checkpoints.jsonl", "ab");
    if (!file) throw std::runtime_error("host generation checkpoint open failed");
    std::fprintf(file,
        "{\"warmup_turn\":%d,\"final\":%s,\"seed\":%u,\"galaxy_turn\":%d,\"galaxy_rng\":%u,\"rtl_rng\":%u,\"stars\":%d,\"planets\":%d,\"ships\":%u,\"star_key_hash\":\"%016llx\",\"planet_key_hash\":\"%016llx\",\"ship_key_hash\":\"%016llx\",\"elapsed_us\":%lld}\n",
        warmup_turn, final ? "true" : "false", g->GenerationSeed, g->CurrentTurn,
        g->RandomState, System::RandSeed, pas::list_count(g->Stars), pas::list_count(g->Planets), ship_count,
        static_cast<unsigned long long>(stars.value), static_cast<unsigned long long>(planets.value),
        static_cast<unsigned long long>(ships.value), static_cast<long long>(us));
    const bool error = std::ferror(file); const int closed = std::fclose(file);
    if (error || closed) throw std::runtime_error("host generation checkpoint write failed");
}
[[noreturn]] inline void SerializeAndStop(aGalaxy::TGalaxy* g) {
    const auto rng = g->RandomState, rtl = System::RandSeed;
    const auto saves = g->SaveCount;
    auto* buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    // This is a real save operation, including native OnSave handlers. No
    // subsequent simulation occurs in this diagnostic process.
    g->SaveToBuffer(buffer);
    FILE* output = std::fopen("generation-world.bin", "wbx");
    if (!output) throw std::runtime_error("host world snapshot open failed");
    const auto bytes = std::fwrite(buffer->Data, 1, buffer->DataSize, output);
    const int closed = std::fclose(output);
    if (bytes != static_cast<std::size_t>(buffer->DataSize) || closed)
        throw std::runtime_error("host world snapshot write failed");
    char detail[256]{};
    std::snprintf(detail, sizeof(detail),
        "HOST generation snapshot complete bytes=%d galaxy_rng_before=%u after=%u rtl_rng_before=%u after=%u save_count_before=%d after=%d",
        buffer->DataSize, rng, g->RandomState, rtl, System::RandSeed, saves, g->SaveCount);
    e2e_stage::Log(detail);
    pas::free(buffer);
    // Match other bounded host probes: terminate all workers only after the
    // private diagnostic outputs have been flushed. Never used on Switch.
    std::_Exit(0);
}
inline void Final(aGalaxy::TGalaxy* galaxy) {
    if (!Seed()) return;
    Checkpoint(galaxy, 300, true);
    if (std::getenv("E2E511_HOST_WORLD_SNAPSHOT")) SerializeAndStop(galaxy);
}
} // namespace srhd_awa::platform::e2e_generation_probe_host
#endif

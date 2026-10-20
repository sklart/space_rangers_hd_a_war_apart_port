#pragma once
#include "types/aGalaxy.hpp"

#if defined(E2E5_HOST_SDL_WINDOW)
#include "e2e5_save_inventory.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Expression.hpp"
#include "units/aPlayer.hpp"
#include "units/aScript.hpp"
#include "units/System.hpp"
#include "types/aItem.hpp"
#include "types/aPlanet.hpp"
#include "units/Globals.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
#include <stdexcept>
#endif

namespace srhd_awa::platform::e2e515_save_checkpoint {
#if defined(E2E5_HOST_SDL_WINDOW)
struct Hash {
    std::uint64_t value{UINT64_C(14695981039346656037)};
    void Byte(std::uint8_t byte) { value ^= byte; value *= UINT64_C(1099511628211); }
    void Word(std::uint32_t word) { for (unsigned i = 0; i < 4; ++i) Byte(word >> (i * 8)); }
    void Bytes(const void* data, std::int32_t size) {
        if (size < 0 || size > 64 * 1024 * 1024 || (size && !data))
            throw std::runtime_error("invalid save checkpoint buffer");
        Word(size);
        for (int i = 0; i < size; ++i) Byte(static_cast<const std::uint8_t*>(data)[i]);
    }
};
inline std::uint64_t Rng(aGalaxy::TGalaxy* galaxy) {
    std::vector<std::array<std::uint32_t, 3>> rows{{0, 0, galaxy->RandomState}};
    for (int i = 0; i < pas::list_count(galaxy->Stars); ++i) {
        const auto* star = pas::list_at<aGalaxy::TStar>(galaxy->Stars, i);
        rows.push_back({1, star->Id, star->RandomState});
        for (int j = 0; j < pas::list_count(star->Ships); ++j) {
            const auto* ship = pas::list_at<aShip::TShip>(star->Ships, j);
            rows.push_back({3, ship->Id, ship->RandomState});
        }
    }
    for (int i = 0; i < pas::list_count(galaxy->Planets); ++i) {
        const auto* planet = pas::list_at<aPlanet::TPlanet>(galaxy->Planets, i);
        rows.push_back({2, planet->Id, planet->RandomState});
    }
    for (int i = 0; i < pas::list_count(galaxy->ShipsInTransit); ++i) {
        const auto* ship = pas::list_at<aShip::TShip>(galaxy->ShipsInTransit, i);
        rows.push_back({3, ship->Id, ship->RandomState});
    }
    std::sort(rows.begin(), rows.end());
    Hash hash; hash.Word(rows.size());
    for (const auto& row : rows) for (auto value : row) hash.Word(value);
    return hash.value;
}
inline void Observe(aGalaxy::TGalaxy* galaxy, const char* phase) {
    if (!std::getenv("E2E515_SAVE_CHECKPOINTS")) return;
    auto* player = aPlayer::GetPlayer();
    if (!galaxy || !player) throw std::runtime_error("save checkpoint missing world/player");
    const auto rtl_rng = System::RandSeed;
    const auto rng_hash = Rng(galaxy);
    e2e5_save_refs::SaveScope save_scope(galaxy);
    auto* buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    pas::ScopeExit cleanup = [&]() noexcept { pas::free(buffer); };
    Hash scripts, globals, inventory, ship;
    player->SaveToBuffer(buffer);
    ship.Bytes(buffer->Data, buffer->DataSize);
    scripts.Word(pas::list_count(galaxy->Scripts));
    for (int i = 0; i < pas::list_count(galaxy->Scripts); ++i) {
        buffer->Clear();
        pas::list_at<aScript::TScript>(galaxy->Scripts, i)->SaveState(buffer);
        scripts.Bytes(buffer->Data, buffer->DataSize);
    }
    globals.Word(Globals::SharedScriptVariables->Count);
    for (int i = 0; i < Globals::SharedScriptVariables->Count; ++i) {
        auto* cell = EC_Expression::TVarArrayEC_GetItem(Globals::SharedScriptVariables, i);
        buffer->Clear();
        if (cell->Name == u"GRunStar") {
            const auto binding = e2e5_save_inventory::classify(galaxy, nullptr, cell);
            if (cell->GetDword() && std::string(binding.object_type) != "star")
                throw std::runtime_error("checkpoint GRunStar is not a stable star");
            buffer->AddWideStringZ(cell->Name);
            buffer->AddDWord(cell->GetDword() ? binding.object_id : 0);
        } else cell->SaveToBuffer(buffer);
        globals.Bytes(buffer->Data, buffer->DataSize);
    }
    // Item order is UI state; compare the complete original serialized items
    // as a multiset, retaining IDs, type and duplicates. No object addresses.
    std::vector<std::vector<std::uint8_t>> items;
    for (int i = 0; i < pas::list_count(player->Inventory); ++i) {
        auto* item = pas::list_at<aItem::TItem>(player->Inventory, i);
        buffer->Clear(); buffer->AddIntegerValue(item->ItemType); item->SaveToBuffer(buffer);
        const auto* bytes = static_cast<const std::uint8_t*>(buffer->Data);
        items.emplace_back(bytes, bytes + buffer->DataSize);
    }
    std::sort(items.begin(), items.end());
    inventory.Word(items.size());
    for (const auto& item : items) inventory.Bytes(item.data(), item.size());
    for (int i = 0; i < 8; ++i) {
        const auto& goods = player->CargoGoods[i];
        inventory.Word(goods.Count); inventory.Word(goods.TotalCost);
        inventory.Word(goods.PurchasedCount); inventory.Word(goods.PurchasedTotalCost);
    }
    if (rtl_rng != System::RandSeed || rng_hash != Rng(galaxy))
        throw std::runtime_error("save checkpoint observation changed RNG");
    std::fprintf(stderr,
        "E2E515_CHECKPOINT|{\"phase\":\"%s\",\"turn\":%u,\"money\":%d,\"ship\":%u,\"star\":%u,\"planet\":%u,"
        "\"scripts\":%d,\"generation_seed\":%u,\"galaxy_rng\":%u,\"player_rng\":%u,"
        "\"rng_hash\":\"%016llx\",\"scripts_hash\":\"%016llx\",\"globals_hash\":\"%016llx\","
        "\"inventory_hash\":\"%016llx\",\"inventory_count\":%zu,\"ship_hash\":\"%016llx\"}\n",
        phase, static_cast<unsigned>(galaxy->CurrentTurn), player->Money, player->Id,
        player->CurrentStar ? player->CurrentStar->Id : 0u,
        player->CurrentPlanet ? player->CurrentPlanet->Id : 0u, pas::list_count(galaxy->Scripts),
        galaxy->GenerationSeed, galaxy->RandomState, player->RandomState,
        static_cast<unsigned long long>(rng_hash), static_cast<unsigned long long>(scripts.value),
        static_cast<unsigned long long>(globals.value), static_cast<unsigned long long>(inventory.value), items.size(),
        static_cast<unsigned long long>(ship.value));
    if (std::string_view(phase).starts_with("trade-") && player->CurrentPlanet) {
        std::fprintf(stderr, "E2E515_TRADE|{\"phase\":\"%s\",\"money\":%d,\"free_space\":%d,\"goods\":[",
            phase, player->Money, player->GetCargoFreeSpace());
        for (int i = 0; i < 8; ++i) {
            const auto& cargo = player->CargoGoods[i];
            std::fprintf(stderr, "%s{\"id\":%d,\"cargo\":%d,\"cost\":%d,\"stock\":%d,\"buy\":%d,\"sell\":%d}",
                i ? "," : "", i, cargo.Count, cargo.TotalCost, player->GetLocationGoodsEntry(i)->Count,
                player->ShopGoodsPurchasePrice(i, nullptr), player->ShopGoodsSellPrice(i, nullptr));
        }
        std::fprintf(stderr, "]}\n");
    }
    std::fflush(stderr);
}
#else
inline void Observe(aGalaxy::TGalaxy*, const char*) {}
#endif
} // namespace srhd_awa::platform::e2e515_save_checkpoint

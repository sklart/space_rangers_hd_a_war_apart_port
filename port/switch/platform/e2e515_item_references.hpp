#pragma once
#include <functional>
#include <unordered_set>
#include "types/aRuins.hpp"

namespace srhd_awa::platform::e2e515_item_references {
// Compare an opaque script value only against owned live objects. Never cast
// or dereference the value itself. IdToItem remains the authoritative inverse.
inline aItem::TItem* Find(aGalaxy::TGalaxy* galaxy, std::uintptr_t value) {
    aItem::TItem* found = nullptr;
    auto count = [](auto* entries) { return entries ? pas::list_count(entries) : 0; };
    std::unordered_set<aShip::TShip*> ships;
    std::unordered_set<aItem::TItem*> items;
    std::function<void(aShip::TShip*)> ship;
    std::function<void(aItem::TItem*)> item;
    auto list = [&](auto* entries) {
        for (int i = 0; !found && i < count(entries); ++i)
            item(pas::list_at<aItem::TItem>(entries, i));
    };
    item = [&](aItem::TItem* candidate) {
        if (!candidate || found || !items.insert(candidate).second) return;
        if (reinterpret_cast<std::uintptr_t>(candidate) == value) { found = candidate; return; }
        if (auto* artefact = pas::class_cast_if<aItem::TArtefactTranclucator*>(candidate))
            ship(static_cast<aShip::TShip*>(artefact->Ship));
    };
    ship = [&](aShip::TShip* candidate) {
        if (!candidate || found || !ships.insert(candidate).second) return;
        list(candidate->Inventory); list(candidate->Artefacts); list(candidate->GuaranteedDeathDropItems);
        if (auto* station = pas::class_cast_if<aRuins::TRuins*>(candidate)) list(station->EquipmentShop);
        if (auto* player = pas::class_cast_if<aPlayer::TPlayer*>(candidate))
            for (int i = 0; !found && i < count(player->StorageEntries); ++i)
                item(pas::list_at<aPlayer::TStorageEntry>(player->StorageEntries, i)->Item);
    };
    for (int i = 0; !found && i < count(galaxy->Stars); ++i) {
        auto* star = pas::list_at<aGalaxy::TStar>(galaxy->Stars, i);
        list(star->Items);
        for (int j = 0; !found && j < count(star->Ships); ++j)
            ship(pas::list_at<aShip::TShip>(star->Ships, j));
        for (int j = 0; !found && j < count(star->Planets); ++j) {
            auto* planet = pas::list_at<aPlanet::TPlanet>(star->Planets, j);
            list(planet->EquipmentShop);
            for (int k = 0; !found && k < count(planet->Warriors); ++k)
                ship(pas::list_at<aShip::TShip>(planet->Warriors, k));
            for (int k = 0; !found && k < count(planet->SurfaceLootEntries); ++k)
                if (auto* loot = pas::list_at<aPlanet::TPlanetSurfaceLootEntry>(planet->SurfaceLootEntries, k)) item(loot->Item);
        }
    }
    for (int i = 0; !found && i < count(galaxy->StoredItems); ++i)
        item(pas::checked_cast<aItem::TItem*>(pas::list_at<aGalaxy::TStoredItem>(galaxy->StoredItems, i)->Item));
    if (!found || found->Id == 0 || galaxy->IdToItem(static_cast<std::uint32_t>(found->Id), false) != found)
        return nullptr;
    return found;
}
}

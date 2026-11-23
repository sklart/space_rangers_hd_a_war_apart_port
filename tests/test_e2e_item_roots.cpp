// Real writer/reader checks for original IdToItem ownership roots.
#include "layout/aGalaxy.hpp"
#include "layout/aPlayer.hpp"
#include "layout/aPlanet.hpp"
#include "layout/aRuins.hpp"
#include "layout/aItem.hpp"
#include "types/aMyFunction.hpp"
#include "types/fEquipmentShop.hpp"
#include "units/fEquipmentShop.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Expression.hpp"
#include "e2e5_save_refs.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::platform;

int main() {
    aGalaxy::TGalaxy galaxy{}; aGalaxy::TStar star{}; aItem::TGoods item{}; item.Id = 765;
    auto* stars = pas::make_object<aMyFunction::TObjectList>();
    auto* constellations = pas::make_object<aMyFunction::TObjectList>();
    auto* planets = pas::make_object<aMyFunction::TObjectList>();
    auto* scripts = pas::make_object<aMyFunction::TObjectList>();
    auto* stored = pas::make_object<aMyFunction::TObjectList>();
    auto* star_items = pas::make_object<aMyFunction::TObjectList>();
    auto* drops = pas::make_object<aMyFunction::TObjectList>();
    auto* ships = pas::make_object<aMyFunction::TObjectList>();
    auto* star_planets = pas::make_object<aMyFunction::TObjectList>();
    auto* slots = pas::make_object<aMyFunction::TObjectList>();
    galaxy.Stars = stars; galaxy.Constellations = constellations; galaxy.Planets = planets;
    galaxy.Scripts = scripts; galaxy.StoredItems = stored;
    star.Items = star_items; star.MovingDropItems = drops; star.Ships = ships; star.Planets = star_planets;
    pas::list_add(stars, &star);
    assert(!fEquipmentShop::TemporaryShopSlots);
    fEquipmentShop::TemporaryShopSlots = slots;
    fEquipmentShop::TShopSlot slot{}; slot.Item = &item; pas::list_add(slots, &slot);
    auto* cell = pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create, EC_Expression::vkDword);
    cell->Name = pas::WideString(u"quest_item");
    const auto address = reinterpret_cast<std::uintptr_t>(&item);
    cell->SetDword(address);
    auto roundtrip = [&]() {
        assert(galaxy.IdToItem(765, false) == &item);
        auto* buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
        bool ok = true;
        try {
            e2e5_save_refs::SaveScope scope(&galaxy);
            e2e5_save_refs::WriteScriptDword(buffer, nullptr, cell);
        } catch (...) { ok = false; }
        if (!ok) { pas::free(buffer); return false; }
        const auto* bytes = static_cast<const unsigned char*>(buffer->Data);
        const auto offset = (cell->Name.length() + 1) * 2;
        assert(buffer->DataSize == offset + 5 && bytes[offset] == e2e5_save_refs::kItem);
        assert(cell->GetDword() == address);
        e2e5_save_refs::SetLoadFormat(true); cell->SetDword(765);
        e2e5_save_refs::QueueLoadedPointer(cell, e2e5_save_refs::kItem, 765);
        e2e5_save_refs::RebindPending(&galaxy);
        assert(cell->GetDword() == address);
        e2e5_save_refs::SetLoadFormat(false); pas::free(buffer); return true;
    };
    if (!roundtrip()) { std::puts("ITEM_ROOT temporary_shop=REJECTED"); return 1; }
    std::puts("ITEM_ROOT temporary_shop=PASS");
    pas::list_clear(slots);
    aGalaxy::TMovingDropItemEntry drop{}; drop.Payload = &item; pas::list_add(drops, &drop);
    if (!roundtrip()) { std::puts("ITEM_ROOT moving_drop=REJECTED"); return 1; }
    std::puts("ITEM_ROOT moving_drop=PASS typed_id_and_reload=PASS");
    fEquipmentShop::TemporaryShopSlots = nullptr;
    pas::free(cell);
    for (auto* entries : {stars, constellations, planets, scripts, stored, star_items, drops, ships, star_planets, slots}) {
        pas::list_clear(entries); pas::free(entries);
    }
}

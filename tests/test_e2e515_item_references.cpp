#include "layout/aGalaxy.hpp"
#include "layout/aPlayer.hpp"
#include "layout/aPlanet.hpp"
#include "layout/aRuins.hpp"
#include "layout/aItem.hpp"
#include "types/aMyFunction.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Expression.hpp"
#include "e2e5_save_refs.hpp"
#include "e2e515_item_references.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace srhd_awa::platform;
int main() {
    aGalaxy::TGalaxy galaxy{};
    aGalaxy::TStar star{};
    aPlayer::TPlayer ship{};
    aItem::TGoods item{};
    item.Id = 77;
    assert(e2e515_item_references::Find(&galaxy, reinterpret_cast<std::uintptr_t>(&item)) == nullptr);
    auto* stars = pas::make_object<aMyFunction::TObjectList>();
    auto* ships = pas::make_object<aMyFunction::TObjectList>();
    auto* inventory = pas::make_object<aMyFunction::TObjectList>();
    galaxy.Stars = stars; star.Ships = ships; ship.Inventory = inventory;
    std::vector<pas::List*> lists{stars, ships, inventory};
    auto empty = [&]() { auto* value = pas::make_object<aMyFunction::TObjectList>(); lists.push_back(value); return value; };
    galaxy.Constellations = empty(); galaxy.Planets = empty(); galaxy.Scripts = empty(); galaxy.StoredItems = empty();
    star.Items = empty(); star.Planets = empty(); star.MovingDropItems = empty();
    ship.Artefacts = empty(); ship.GuaranteedDeathDropItems = empty(); ship.StorageEntries = empty();
    pas::list_add(stars, &star); pas::list_add(ships, &ship); pas::list_add(inventory, &item);
    const auto address = reinterpret_cast<std::uintptr_t>(&item);
    assert(e2e515_item_references::Find(&galaxy, address) == &item);
    assert(e2e515_item_references::Find(&galaxy, UINT64_C(0x123456789abcdef0)) == nullptr);
    auto* cell = pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create, EC_Expression::vkDword);
    auto* other = pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create, EC_Expression::vkDword);
    auto* buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    cell->Name = pas::WideString(u"quest_item"); cell->SetDword(address);
    {
        e2e5_save_refs::SaveScope scope(&galaxy);
        e2e5_save_refs::WriteScriptDword(buffer, nullptr, cell);
    }
    const auto* bytes = static_cast<const unsigned char*>(buffer->Data);
    const auto offset = (cell->Name.length() + 1) * 2;
    assert(buffer->DataSize == offset + 5 && bytes[offset] == e2e5_save_refs::kItem);
    assert(bytes[offset + 1] == 77 && bytes[offset + 2] == 0 && bytes[offset + 3] == 0 && bytes[offset + 4] == 0);
    assert(cell->GetDword() == address); // Saving never changes the live variable.
    e2e5_save_refs::SetLoadFormat(true);
    cell->SetDword(77);
    e2e5_save_refs::QueueLoadedPointer(cell, e2e5_save_refs::kItem, 77);
    e2e5_save_refs::RebindPending(&galaxy);
    assert(cell->GetDword() == address);
    // A missing later item must not partially rebind earlier cells.
    cell->SetDword(123); other->SetDword(456);
    e2e5_save_refs::QueueLoadedPointer(cell, e2e5_save_refs::kItem, 77);
    e2e5_save_refs::QueueLoadedPointer(other, e2e5_save_refs::kItem, 9999);
    bool rejected = false;
    try { e2e5_save_refs::RebindPending(&galaxy); } catch (...) { rejected = true; }
    assert(rejected && cell->GetDword() == 123 && other->GetDword() == 456);
    e2e5_save_refs::SetLoadFormat(false);
    rejected = false;
    try { e2e5_save_refs::QueueLoadedPointer(cell, e2e5_save_refs::kItem, 77); } catch (...) { rejected = true; }
    assert(rejected); // Never reinterpret this extension as a legacy v167 value.
    pas::free(cell); pas::free(other); pas::free(buffer);
    for (auto* entries : lists) { pas::list_clear(entries); pas::free(entries); }
    std::puts("PASS live item identity, typed little-endian ID, roundtrip, missing-ID atomic rejection and legacy guard");
}

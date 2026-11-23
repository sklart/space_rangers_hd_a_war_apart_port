// Verify a script variable can refer to a live galaxy-owned ship even when
// that ship is not bound to the current script. No opaque value is dereferenced.
#include "layout/aGalaxy.hpp"
#include "layout/aPlayer.hpp"
#include "layout/aPlanet.hpp"
#include "layout/aRuins.hpp"
#include "layout/aItem.hpp"
#include "types/aMyFunction.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Expression.hpp"
#include "e2e5_save_refs.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::platform;

int main() {
    aGalaxy::TGalaxy galaxy{};
    aGalaxy::TStar star{};
    aPlayer::TPlayer other_ship{};
    other_ship.Id = 456;
    auto* stars = pas::make_object<aMyFunction::TObjectList>();
    auto* ships = pas::make_object<aMyFunction::TObjectList>();
    auto* constellations = pas::make_object<aMyFunction::TObjectList>();
    auto* planets = pas::make_object<aMyFunction::TObjectList>();
    auto* scripts = pas::make_object<aMyFunction::TObjectList>();
    auto* stored = pas::make_object<aMyFunction::TObjectList>();
    auto* star_planets = pas::make_object<aMyFunction::TObjectList>();
    galaxy.Stars = stars; galaxy.Constellations = constellations;
    galaxy.Planets = planets; galaxy.Scripts = scripts; galaxy.StoredItems = stored;
    star.Ships = ships; star.Planets = star_planets;
    pas::list_add(stars, &star); pas::list_add(ships, &other_ship);
    assert(galaxy.IdToShip(456, false) == &other_ship);
    auto* cell = pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create, EC_Expression::vkDword);
    auto* buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    cell->Name = pas::WideString(u"quest_ship");
    const auto address = reinterpret_cast<std::uintptr_t>(&other_ship);
    cell->SetDword(address);
    bool serialized = true;
    try {
        e2e5_save_refs::SaveScope scope(&galaxy);
        e2e5_save_refs::WriteScriptDword(buffer, nullptr, cell);
    } catch (...) { serialized = false; }
    if (!serialized) {
        std::puts("SHIP_REFERENCE live_unbound_galaxy_ship=REJECTED");
        return 1;
    }
    const auto* bytes = static_cast<const unsigned char*>(buffer->Data);
    const auto offset = (cell->Name.length() + 1) * 2;
    assert(buffer->DataSize == offset + 5 && bytes[offset] == e2e5_save_refs::kShip);
    assert(bytes[offset + 1] == 0xc8 && bytes[offset + 2] == 0x01 && bytes[offset + 3] == 0 && bytes[offset + 4] == 0);
    assert(cell->GetDword() == address);
    e2e5_save_refs::SetLoadFormat(true);
    cell->SetDword(456);
    e2e5_save_refs::QueueLoadedPointer(cell, e2e5_save_refs::kShip, 456);
    e2e5_save_refs::RebindPending(&galaxy);
    assert(cell->GetDword() == address);
    e2e5_save_refs::SetLoadFormat(false);
    auto rejected = [&]() {
        auto* scratch = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
        bool failed = false;
        try {
            e2e5_save_refs::SaveScope scope(&galaxy);
            e2e5_save_refs::WriteScriptDword(scratch, nullptr, cell);
        } catch (...) { failed = true; }
        pas::free(scratch);
        return failed;
    };
    other_ship.Id = 0;
    assert(rejected()); // A live address without a stable ID stays rejected.
    other_ship.Id = 456;
    aPlayer::TPlayer collision{}; collision.Id = 456;
    pas::list_clear(ships); pas::list_add(ships, &collision); pas::list_add(ships, &other_ship);
    assert(galaxy.IdToShip(456, false) == &collision && rejected());
    cell->SetDword(UINT64_C(0x123456789abcdef0));
    assert(rejected()); // Never dereference or truncate an unknown address.
    pas::list_clear(ships);
    aPlanet::TPlanet planet{};
    auto* warriors = pas::make_object<aMyFunction::TObjectList>();
    planet.Warriors = warriors;
    pas::list_add(star_planets, &planet); pas::list_add(warriors, &other_ship);
    cell->SetDword(address);
    auto* warrior_buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    {
        e2e5_save_refs::SaveScope scope(&galaxy);
        e2e5_save_refs::WriteScriptDword(warrior_buffer, nullptr, cell);
    }
    const auto* warrior_bytes = static_cast<const unsigned char*>(warrior_buffer->Data);
    assert(warrior_bytes[offset] == e2e5_save_refs::kShip);
    e2e5_save_refs::SetLoadFormat(true);
    cell->SetDword(456);
    e2e5_save_refs::QueueLoadedPointer(cell, e2e5_save_refs::kShip, 456);
    e2e5_save_refs::RebindPending(&galaxy);
    assert(cell->GetDword() == address);
    e2e5_save_refs::SetLoadFormat(false);
    pas::free(warrior_buffer); pas::list_clear(warriors); pas::free(warriors);
    pas::free(cell); pas::free(buffer);
    for (auto* entries : {stars, ships, constellations, planets, scripts, stored, star_planets}) {
        pas::list_clear(entries); pas::free(entries);
    }
    std::puts("SHIP_REFERENCE live_unbound_galaxy_ship=PASS typed_id_and_reload=PASS warrior_zero_duplicate_unknown=PASS");
}

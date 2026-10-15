#include "types/aGalaxy.hpp"
#include "types/aPlanet.hpp"
#include "types/aShip.hpp"
#include "types/aRuins.hpp"
#include "types/aRanger.hpp"
#include "types/aPlayer.hpp"

#include <cstddef>

// These are the exact excluded live pointers in XorProtectedState. Check
// native C++ layouts, since 32-bit Delphi offsets cannot be reused on ARM64.
static_assert(offsetof(aGalaxy::TStar, Planets) < offsetof(aGalaxy::TStar, Asteroids));
static_assert(offsetof(aGalaxy::TStar, Asteroids) < offsetof(aGalaxy::TStar, Ships));
static_assert(offsetof(aGalaxy::TStar, Ships) < offsetof(aGalaxy::TStar, Items));
static_assert(offsetof(aGalaxy::TStar, Items) < offsetof(aGalaxy::TStar, MovingDropItems));
static_assert(offsetof(aGalaxy::TStar, MovingDropItems) + sizeof(void*) <= sizeof(aGalaxy::TStar));
static_assert(offsetof(aPlanet::TPlanet, EquipmentShop) + sizeof(void*) <= sizeof(aPlanet::TPlanet));
static_assert(offsetof(aShip::TShip, Inventory) < offsetof(aShip::TShip, Artefacts));
static_assert(offsetof(aShip::TShip, Artefacts) < offsetof(aRuins::TRuins, EquipmentShop));
static_assert(offsetof(aShip::TShip, Artefacts) < offsetof(aRanger::TRanger, Quests));
static_assert(offsetof(aRanger::TRanger, Quests) < offsetof(aPlayer::TPlayer, StorageEntries));
static_assert(offsetof(aPlayer::TPlayer, StorageEntries) < offsetof(aPlayer::TPlayer, Satellites));
static_assert(offsetof(aPlayer::TPlayer, Satellites) + sizeof(void*) <= sizeof(aPlayer::TPlayer));

int main() {}

// Original memory-only battle prerequisite. Never invokes save-file APIs.
#include "units/GI_MessageLoop.hpp"
void ExerciseOriginalSnapshot(m::RangersCampaignSession& campaign){
 auto* world=aGalaxy::Galaxy;auto* player=aPlayer::GetPlayer();auto* gov=Globals::GovernmentScreen;
 assert(world && player && player->CurrentPlanet && gov && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
 const auto turn=world->CurrentTurn,money=player->Money,saves=world->SaveCount,loads=world->LoadCount;
 const auto id=player->Id;const auto planet_id=player->CurrentPlanet->Id;
 const auto name=player->Name;const auto stars=pas::list_count(world->Stars),planets=pas::list_count(world->Planets),storage=pas::list_count(player->StorageEntries);
 const auto previous=GlobalsV::CurrentScreenId;GlobalsV::CurrentScreenId=GlobalsV::screenGovernment;
 auto* previous_stack=GI_MessageLoop::MessageLoopStack;GI_MessageLoop::PushMessageLoop(gov);
 gov->OnOpen();std::puts("SNAPSHOT original_SaveGameToMemorySnapshot_begin");
 aSaveLoad::SaveGameToMemorySnapshot();
 assert(GlobalsV::MemorySnapshotActive && !aGalaxy::Galaxy && aSaveLoad::MemorySnapshotBuffer);
 Reject([&]{(void)campaign.Galaxy();});Reject([]{m::RangersCampaignSession duplicate;});
 std::puts("SNAPSHOT detached_world=PASS original_RestoreGameFromMemorySnapshot_begin");
 aSaveLoad::RestoreGameFromMemorySnapshot();
 assert(!GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer && aGalaxy::Galaxy && aPlayer::GetPlayer());
 world=aGalaxy::Galaxy;player=aPlayer::GetPlayer();
 assert(&campaign.Galaxy()==world);
 assert(player->Id==id && player->Name==name && player->Money==money && player->CurrentPlanet && player->CurrentPlanet->Id==planet_id);
 assert(world->CurrentTurn==turn && world->SaveCount==saves && world->LoadCount==loads && pas::list_count(world->Stars)==stars && pas::list_count(world->Planets)==planets && pas::list_count(player->StorageEntries)==storage);
 gov->OnClose();GI_MessageLoop::PopMessageLoop(gov);
 if(!previous_stack){assert(pas::list_count(GI_MessageLoop::MessageLoopStack)==0);pas::free(GI_MessageLoop::MessageLoopStack);GI_MessageLoop::MessageLoopStack=nullptr;}
 GlobalsV::CurrentScreenId=previous;
 std::printf("SNAPSHOT original_memory_roundtrip=PASS turn=%d player_id=%u planet_id=%u money=%d storage=%d persistent_save_load_counters_unchanged=PASS no_save_files=PASS\n",turn,unsigned(id),unsigned(planet_id),money,storage);
}

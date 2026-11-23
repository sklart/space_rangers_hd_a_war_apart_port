// Fail original in-memory snapshot preparation before world detachment.
auto* gov = Globals::GovernmentScreen; gov->InitializeLayout();
const auto previous_screen_id = GlobalsV::CurrentScreenId;
auto* previous_stack = GI_MessageLoop::MessageLoopStack;
GlobalsV::CurrentScreenId = GlobalsV::screenGovernment;
GI_MessageLoop::PushMessageLoop(gov); gov->OnOpen(); gov->PendingTransition = 1;
const bool expect_clean = std::getenv("SRHD_EXPECT_CLEAN_BUFFERS") != nullptr;
for (int point : {4, 5}) {
 auto* original_world = aGalaxy::Galaxy;
 const auto original_count = original_world->SaveCount;
 const auto original_player = aPlayer::GetPlayer()->Id;
 const auto original_money = aPlayer::GetPlayer()->Money;
 const auto before = matrixgame_save_buffer_probe::live.load();
 matrixgame_save_buffer_probe::fail_before = point;
 bool caught = false;
 try { aSaveLoad::SaveGameToMemorySnapshot(); } catch (const pas::Raised&) { caught = true; }
 matrixgame_save_buffer_probe::fail_before = 0;
 const auto delta = matrixgame_save_buffer_probe::live.load() - before;
 assert(caught && aGalaxy::Galaxy == original_world && !GlobalsV::MemorySnapshotActive);
 assert(aPlayer::GetPlayer()->Id == original_player && aPlayer::GetPlayer()->Money == original_money);
 const auto count_delta = original_world->SaveCount - original_count;
 std::printf("SNAPSHOT_PREP_FAILURE point=%d live_delta=%ld buffer=%d save_count_delta=%d\n",
             point, delta, aSaveLoad::MemorySnapshotBuffer != nullptr, count_delta);
 if (expect_clean) {
  assert(delta == 0 && !aSaveLoad::MemorySnapshotBuffer && count_delta == 0);
 } else {
  // Dispose only the test fixture's observed partial buffer so the next
  // independent boundary can run; this is not a product recovery path.
  if (aSaveLoad::MemorySnapshotBuffer) pas::free(aSaveLoad::MemorySnapshotBuffer);
  aSaveLoad::MemorySnapshotBuffer = nullptr; original_world->SaveCount = original_count;
 }
}
if (expect_clean) {
 auto* existing = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
 aSaveLoad::MemorySnapshotBuffer = existing;
 bool rejected = false;
 try { aSaveLoad::SaveGameToMemorySnapshot(); } catch (const pas::Raised&) { rejected = true; }
 assert(rejected && aSaveLoad::MemorySnapshotBuffer == existing && !GlobalsV::MemorySnapshotActive);
 pas::free(existing); aSaveLoad::MemorySnapshotBuffer = nullptr;
 const auto id = aPlayer::GetPlayer()->Id; const auto money = aPlayer::GetPlayer()->Money;
 const auto count = aGalaxy::Galaxy->SaveCount; const auto turn = aGalaxy::Galaxy->CurrentTurn;
 aSaveLoad::SaveGameToMemorySnapshot();
 assert(GlobalsV::MemorySnapshotActive && aSaveLoad::MemorySnapshotBuffer && !aGalaxy::Galaxy);
 aSaveLoad::RestoreGameFromMemorySnapshot();
 assert(!GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer && aGalaxy::Galaxy);
 assert(aPlayer::GetPlayer()->Id == id && aPlayer::GetPlayer()->Money == money &&
        aGalaxy::Galaxy->SaveCount == count && aGalaxy::Galaxy->CurrentTurn == turn);
 std::puts("SNAPSHOT_PREP subsequent_real_snapshot_restore=PASS existing_snapshot_preserved=PASS");
}
gov->PendingTransition = 0; gov->OnClose();
GI_MessageLoop::PopMessageLoop(gov);
if (!previous_stack) {
 assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 0);
 pas::free(GI_MessageLoop::MessageLoopStack); GI_MessageLoop::MessageLoopStack = nullptr;
}
GlobalsV::CurrentScreenId = previous_screen_id;

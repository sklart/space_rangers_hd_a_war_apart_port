// Exercise the actual save preflight, with a deliberately unresolvable value.
auto* gov = Globals::GovernmentScreen; gov->InitializeLayout();
const auto previous_screen_id = GlobalsV::CurrentScreenId;
auto* previous_stack = GI_MessageLoop::MessageLoopStack;
GlobalsV::CurrentScreenId = GlobalsV::screenGovernment;
GI_MessageLoop::PushMessageLoop(gov); gov->OnOpen(); gov->PendingTransition = 1;
EC_Expression::TVarEC* cell = nullptr;
for (int si = 0; si < pas::list_count(aGalaxy::Galaxy->Scripts); ++si) {
 auto* script = pas::list_at<aScript::TScript>(aGalaxy::Galaxy->Scripts, si);
 if (script->ScriptFileName != u"Script.PC_fem_rangers"_w) continue;
 auto* vars = script->InitCode->LocalVar;
 for (int vi = 0; vi < vars->Count; ++vi) {
  auto* candidate = EC_Expression::TVarArrayEC_GetItem(vars, vi);
  if (candidate->Name == u"quest_item"_w && candidate->Kind == EC_Expression::vkDword) cell = candidate;
 }
}
assert(cell);
const auto saved_value = cell->GetDword();
cell->SetDword(static_cast<std::uintptr_t>(0x123456789abcdef0ULL));
auto* world = aGalaxy::Galaxy; const auto count = world->SaveCount;
const auto id = aPlayer::GetPlayer()->Id; const auto money = aPlayer::GetPlayer()->Money;
const auto turn = world->CurrentTurn; const auto before = matrixgame_save_buffer_probe::live.load();
bool caught = false;
try { aSaveLoad::SaveGameToMemorySnapshot(); } catch (const pas::Raised&) { caught = true; }
assert(caught && world == aGalaxy::Galaxy && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
assert(world->SaveCount == count && world->CurrentTurn == turn && aPlayer::GetPlayer()->Id == id && aPlayer::GetPlayer()->Money == money);
assert(matrixgame_save_buffer_probe::live.load() == before);
assert(cell->GetDword() == static_cast<std::uintptr_t>(0x123456789abcdef0ULL));
cell->SetDword(saved_value);
std::puts("SNAPSHOT_UNKNOWN classifier_rejected_world_and_buffer_restored=PASS");
aSaveLoad::SaveGameToMemorySnapshot();
assert(GlobalsV::MemorySnapshotActive && !aGalaxy::Galaxy);
aSaveLoad::RestoreGameFromMemorySnapshot();
assert(aGalaxy::Galaxy && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
assert(aPlayer::GetPlayer()->Id == id && aPlayer::GetPlayer()->Money == money && aGalaxy::Galaxy->CurrentTurn == turn && aGalaxy::Galaxy->SaveCount == count);
std::puts("SNAPSHOT_UNKNOWN subsequent_real_snapshot_restore=PASS");
gov->PendingTransition = 0; gov->OnClose(); GI_MessageLoop::PopMessageLoop(gov);
if (!previous_stack) { assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 0); pas::free(GI_MessageLoop::MessageLoopStack); GI_MessageLoop::MessageLoopStack = nullptr; }
GlobalsV::CurrentScreenId = previous_screen_id;

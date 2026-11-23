// Exercise an actual native-engine callback failure without manufacturing a result.
Globals::GalaxyScreen->InitializeLayout();
Globals::PlanetScreen->InitializeLayout();
auto* gov = Globals::GovernmentScreen; gov->InitializeLayout();
int map_index = -1;
for (int i = 0; i < Globals::RobotMapDefinitions.length(); ++i) {
 std::u16string path(pas::view(Globals::RobotMapDefinitions[i].Map));
 for (auto& ch : path) if (ch >= u'A' && ch <= u'Z') ch += u'a' - u'A';
 std::u16string_view name(path); const auto slash = name.find_last_of(u"/\\");
 if (slash != name.npos) name.remove_prefix(slash + 1);
 if (name == u"winter1_1e.cmap") { assert(map_index < 0); map_index = i; }
}
assert(map_index >= 0);
const int map_id = Globals::RobotMapDefinitions[map_index].Id;
RangersCallerPresentation caller_presentation(window, device, [&]{return window.Attach(&platform3g::WindowProcedure);});
Driver battle; native_loop::Scope loop(battle);
int stage = 0, history_before = 0, money_before = 0, turn_before = 0, player_id = 0, planet_id = 0, battles_before = 0;
bool cancel_sent = false, caught = false;
m::RangersNativeInputBackend input_backend;
input_backend.poll = [&] {
 auto* stack = GI_MessageLoop::MessageLoopStack;
 if (!stack || !pas::list_count(stack)) return;
 auto* current = static_cast<GI_MessageLoop::TMessageLoopGI*>(pas::list_get(stack, pas::list_count(stack)-1));
 if (current->ExitCode) return;
 if (stage == 0 && GlobalsV::CurrentScreenId == GlobalsV::screenPlanet && current == Globals::PlanetScreen) {
  Globals::PlanetScreen->PlanetPanel->GovernmentClicked(nullptr);
  if (GlobalsV::RequestedScreenId == GlobalsV::screenGovernment) { stage = 1; std::puts("FAILURE planet_to_government=PASS"); }
  return;
 }
 if (stage == 1 && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment && current == gov) {
  auto* player = aPlayer::GetPlayer(); history_before = player->PlanetBattleHistory.length(); money_before = player->Money;
  turn_before = campaign.Galaxy().CurrentTurn; player_id = player->Id; planet_id = player->CurrentPlanet->Id; battles_before = player->PlanetBattles;
  GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness, GR_Main::DisplayContrast); CaptureCallerGammaBeforeBattle();
  stage = 2; gov->StartPlanetBattleWithBombardment(map_id);
  assert(gov->PendingTransition == 1); std::puts("FAILURE original_Gov_start=PASS"); return;
 }
 if (stage == 2 && battle.pumps && current != gov && !cancel_sent) {
  auto* target = native_input::CurrentUiTarget(); assert(target);
  cancel_sent = true; target->Key(true, 27); target->Key(false, 27);
  std::puts("FAILURE error_dialog_cancel=INPUT");
 }
};
input_backend.wait_for_drawable = []{}; input_backend.reset_input = []{};
input_backend.drawable = []{return true;}; input_backend.extent = []{return std::pair{1024,768};};
input_backend.warp = [](int,int){};
m::RangersNativeInput input(input_backend);
GlobalsV::RequestedScreenId = GlobalsV::screenPlanet;
try { Globals::RunMainScreenStateLoop(); }
catch (const pas::Raised&) { caught = true; std::puts("FAILURE original_Pascal_exception=CAUGHT"); }
assert(caught && cancel_sent && battle.pumps == 1);
assert(caller_presentation.WindowOwned() && !caller_presentation.BattleActive());
VerifyCallerGammaAfterBattle(); MatrixClean();
assert(!GR_Main::RobotBattleActive && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
auto* player = aPlayer::GetPlayer();
assert(player && player->Id == player_id && player->CurrentPlanet && player->CurrentPlanet->Id == planet_id && campaign.Galaxy().CurrentTurn == turn_before);
assert(player->Money == money_before && player->PlanetBattles == battles_before && player->PlanetBattleHistory.length() == history_before);
assert(!GR_Main::ExitScreenLoop);
GlobalsV::RequestedScreenId = GlobalsV::screenNone;
std::puts("FAILURE native_callback_snapshot_window_campaign_recovered=PASS no_fake_result=PASS");

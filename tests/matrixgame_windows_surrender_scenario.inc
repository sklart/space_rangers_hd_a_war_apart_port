// Run the real screen dispatcher and select surrender through MatrixGame input.
Globals::GalaxyScreen->InitializeLayout();
Globals::PlanetScreen->InitializeLayout();
auto* gov = Globals::GovernmentScreen;
gov->InitializeLayout();
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
m::RangersNativeInputBackend input_backend;
input_backend.poll = [&] {
 auto* stack = GI_MessageLoop::MessageLoopStack;
 if (!stack || !pas::list_count(stack)) return;
 auto* current = static_cast<GI_MessageLoop::TMessageLoopGI*>(pas::list_get(stack, pas::list_count(stack)-1));
 if (current->ExitCode) return;
 if (stage == 0 && GlobalsV::CurrentScreenId == GlobalsV::screenPlanet && current == Globals::PlanetScreen) {
  Globals::PlanetScreen->PlanetPanel->GovernmentClicked(nullptr);
  if (GlobalsV::RequestedScreenId == GlobalsV::screenGovernment) { stage = 1; std::puts("SURRENDER planet_to_government=PASS"); }
  return;
 }
 if (stage == 1 && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment && current == gov) {
  auto* player = aPlayer::GetPlayer();
  history_before = player->PlanetBattleHistory.length(); money_before = player->Money;
  turn_before = campaign.Galaxy().CurrentTurn; player_id = player->Id;
  planet_id = player->CurrentPlanet->Id; battles_before = player->PlanetBattles;
  GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness, GR_Main::DisplayContrast); CaptureCallerGammaBeforeBattle();
  stage = 2; gov->StartPlanetBattleWithBombardment(map_id);
  assert(gov->PlanetBattleMapId == map_id && fGov::GovernmentBattleDifficulty == 3 && gov->PendingTransition == 1);
  std::puts("SURRENDER original_Gov_start=PASS"); return;
 }
 if (stage == 2 && battle.natural_outcome == 4 && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment && current == gov && gov->PendingTransition == 0) {
  auto* player = aPlayer::GetPlayer();
  assert(caller_presentation.WindowOwned() && !caller_presentation.BattleActive());
  VerifyCallerGammaAfterBattle(); MatrixClean();
  assert(!GR_Main::RobotBattleActive && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
  assert(player->Id == player_id && player->CurrentPlanet && player->CurrentPlanet->Id == planet_id && campaign.Galaxy().CurrentTurn == turn_before);
  assert(player->Money == money_before && player->PlanetBattles == battles_before && player->PlanetBattleHistory.length() == history_before + 1);
  assert(player->CurrentPlanet->GetRelationLevelToShip(player) == aGalaxyStruct::rlBad && gov->DialogText.length() > 0);
  const auto& actual = Robot::RobotBattleStatistics; const auto& expected = battle.expected;
  assert(actual.SignedTimeMs == expected.m_Time && actual.RobotsBuilt == expected.m_BuildRobot && actual.RobotsDestroyed == expected.m_KillRobot &&
   actual.TurretsBuilt == expected.m_BuildTurret && actual.TurretsDestroyed == expected.m_KillTurret && actual.BuildingsDestroyed == expected.m_KillBuilding);
  const auto& record = player->PlanetBattleHistory[history_before];
  assert(record.MapId == map_id && record.CompletionMode == 4 && record.ResultCode == 3 && record.DateTurn == turn_before);
  assert(record.Statistics.SignedTimeMs == actual.SignedTimeMs && record.Statistics.RobotsBuilt == actual.RobotsBuilt &&
   record.Statistics.RobotsDestroyed == actual.RobotsDestroyed && record.Statistics.TurretsBuilt == actual.TurretsBuilt &&
   record.Statistics.TurretsDestroyed == actual.TurretsDestroyed && record.Statistics.BuildingsDestroyed == actual.BuildingsDestroyed);
  std::puts("SURRENDER original_Gov_relation_no_reward_history_statistics=PASS");
  stage = 3; gov->ReturnToPlanet(0); return;
 }
 if (stage == 3 && GlobalsV::CurrentScreenId == GlobalsV::screenPlanet && current == Globals::PlanetScreen) {
  stage = 4; std::puts("SURRENDER dispatcher_return_to_planet=PASS"); GR_Main::ExitScreenLoop = true;
 }
};
input_backend.wait_for_drawable = []{}; input_backend.reset_input = []{};
input_backend.drawable = []{return true;}; input_backend.extent = []{return std::pair{1024,768};};
input_backend.warp = [](int,int){};
m::RangersNativeInput input(input_backend);
GlobalsV::RequestedScreenId = GlobalsV::screenPlanet;
Globals::RunMainScreenStateLoop();
assert(stage == 4 && GR_Main::ExitScreenLoop);
GR_Main::ExitScreenLoop = false; GlobalsV::RequestedScreenId = GlobalsV::screenNone;
std::puts("SURRENDER original_screen_state_loop=PASS");

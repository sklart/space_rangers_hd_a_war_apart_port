// Included inside an isolated Windows reference campaign. The input is a copy
// of a prior dispatcher autosave, never the user's save or the original file.
const char* expected_text = std::getenv("SRHD_TEST_EXPECTED_STATE");
assert(expected_text);
unsigned expected_turn = 0, expected_id = 0, expected_star = 0, expected_planet = 0;
int expected_money = 0, expected_scripts = 0, expected_history = 0;
assert(std::sscanf(expected_text, "%u,%d,%u,%u,%u,%d,%d", &expected_turn, &expected_money,
                   &expected_id, &expected_star, &expected_planet, &expected_scripts, &expected_history) == 7);
const auto path = fSaveManager::TfSaveManager::GetAutoSavePath();
assert(aSaveLoad::LoadGameFromFile(path));
auto* loaded = aPlayer::GetPlayer();
assert(loaded && aGalaxy::Galaxy && loaded->CurrentStar && loaded->CurrentPlanet);
assert(aGalaxy::Galaxy->CurrentTurn == expected_turn && loaded->Money == expected_money && loaded->Id == expected_id &&
       loaded->CurrentStar->Id == expected_star && loaded->CurrentPlanet->Id == expected_planet &&
       pas::list_count(aGalaxy::Galaxy->Scripts) == expected_scripts && loaded->PlanetBattleHistory.length() == expected_history);
if (expected_history) {
 const auto& record = loaded->PlanetBattleHistory[expected_history - 1];
 assert(record.MapId == 12 && record.CompletionMode == 2 && record.ResultCode == 3 && record.DateTurn == expected_turn);
}
std::printf("AUTOSAVE_RELOAD original_load=PASS turn=%u money=%d player=%u star=%u planet=%u scripts=%d history=%d\n",
            expected_turn, expected_money, expected_id, expected_star, expected_planet, expected_scripts, expected_history);

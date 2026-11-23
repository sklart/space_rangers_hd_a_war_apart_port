"""Build the original screen dispatcher around the preserved D3D9 battle fixture."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from matrixgame_windows_host_env import environment

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    surrender = sys.argv[1:] == ['--surrender']
    failure = sys.argv[1:] == ['--failure']
    oom = sys.argv[1:] == ['--oom']
    missing_map = sys.argv[1:] == ['--missing-map']
    concurrency = sys.argv[1:] == ['--concurrent-save']
    classification = sys.argv[1:] == ['--classification-save']
    worker_failure = sys.argv[1:] == ['--worker-failure'] or concurrency
    save_retry = sys.argv[1:] == ['--save-retry'] or worker_failure or classification
    unknown_transition = sys.argv[1:] == ['--unknown-transition']
    snapshot_regression = sys.argv[1:] == ['--snapshot-regression'] or unknown_transition or save_retry
    savefix_regression = sys.argv[1:] == ['--savefix-regression'] or snapshot_regression
    fixed_regression = sys.argv[1:] == ['--fixed-regression'] or savefix_regression
    failure = failure or oom or missing_map or unknown_transition
    menu = sys.argv[1:] == ['--menu']
    if sys.argv[1:] and not (surrender or failure or menu or fixed_regression):
        raise RuntimeError('unknown arguments')
    out = BASE / ('gov-frun-classification-save-link-1' if classification else 'gov-frun-concurrent-save-link-1' if concurrency else 'gov-frun-worker-failure-link-1' if worker_failure else 'gov-frun-save-retry-link-1' if save_retry else 'gov-frun-unknown-transition-link-1' if unknown_transition else 'gov-frun-snapshot-regression-link-1' if snapshot_regression else 'gov-frun-savefix-regression-link-1' if savefix_regression else 'gov-frun-fixed-regression-link-2' if fixed_regression else 'gov-frun-missing-map-link-5' if missing_map else 'gov-frun-oom-link-1' if oom else 'gov-frun-menu-link-2' if menu else 'gov-frun-failure-link-1' if failure else 'gov-frun-surrender-link-2' if surrender else 'gov-frun-dispatcher-link-2')
    if out.exists():
        raise RuntimeError('output already exists')
    out.mkdir()
    env = environment(out)
    prior = BASE / 'gov-frun-link-3'
    old = json.loads((prior / 'result.json').read_text(encoding='utf8'))
    result = {'status': 'INCOMPLETE', 'commands': []}

    def run(label, command):
        command = list(map(str, command))
        process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
        (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
        result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
        print(label, process.returncode, flush=True)
        if process.returncode:
            raise RuntimeError('see ' + label + '.log')

    try:
        original = BASE / 'gov-frun-two-link/campaign_frun_two.cpp'
        source_text = original.read_text(encoding='utf8')
        anchor = '#include "units/fGov.hpp"'
        assert source_text.count(anchor) >= 1
        source_text = source_text.replace(anchor, anchor + '\n#include "types/fGalaxy2.hpp"\n#include "types/fPlanet.hpp"\n#include "types/fPanelPlanet.hpp"\n#include "rangers_native_input.hpp"\n#include "native_ui_target.hpp"', 1)
        if fixed_regression:
            source_text = source_text.replace(anchor, anchor + '\n#include "rangers_session_log.hpp"', 1)
            log_anchor = ' HostClean();MatrixClean();Empty();const auto cwd=std::filesystem::current_path();'
            assert source_text.count(log_anchor) == 1
            log_setup = (' auto logdir=std::filesystem::path(output_root)/"rangers-user"/"logs";'
                         'std::filesystem::create_directories(logdir);'
                         'm::InitializeRangersLog((logdir/"game.log").string().c_str());')
            source_text = source_text.replace(log_anchor, log_anchor + '\n' + log_setup, 1)
        if unknown_transition or save_retry:
            source_text = source_text.replace(anchor, anchor + '\n#include "types/aScript.hpp"\n#include "units/EC_Expression.hpp"', 1)
        if worker_failure or classification:
            source_text = source_text.replace(anchor, anchor + '\n#include "matrixgame_worker_failure_probe.hpp"\n#include "e2e5_save_commit_wait.hpp"\n#include "types/fSaveManager.hpp"\n#include <fstream>\n#include <iterator>', 1)
        if concurrency:
            source_text = source_text.replace(anchor, anchor + '\n#include "matrixgame_save_concurrency_probe.hpp"\n#include "e2e4_quicksave_transaction.hpp"\n#include "types/aSaveLoad.hpp"\n#include <thread>', 1)
        if menu:
            extra = '\n#include "rangers_achievement_session.hpp"\n#include "rangers_world_shutdown.hpp"\n#include "units/MessageText.hpp"\n#include "units/GI_GraphButton.hpp"\n#include "types/fMainForm.hpp"\n#include "types/fGameSettings2.hpp"\n#include "types/fRuinsTalk.hpp"\n#include "types/fPanelRuins.hpp"\n#include "types/fHangar.hpp"\n#include "types/fStarMap.hpp"\n#include <chrono>\n#include <thread>\n'
            source_text = source_text.replace(anchor, anchor + extra, 1)
            setup = '   m::RangersCampaignOptions campaign_options;campaign_options.difficulty.fill(MATRIXGAME_CAMPAIGN_DIFFICULTY);\n   m::RangersCampaignSession campaign(campaign_options);ExerciseOriginalLanding();\n'
            assert source_text.count(setup) == 1
            source_text = source_text.replace(setup, '   m::RangersAchievementSession achievements;\n   auto* messages=pas::construct_call<MessageText::TQuestMessages>(MessageText::TQuestMessages_Create);assert(!MessageText::QuestMessages);MessageText::QuestMessages=messages;\n', 1)
            initial_screen = 'GlobalsV::CurrentScreenId=GlobalsV::screenIntroduction;'
            assert source_text.count(initial_screen) == 1
            source_text = source_text.replace(initial_screen, 'GlobalsV::CurrentScreenId=GlobalsV::screenNone;', 1)
        if surrender or failure:
            pump_start = source_text.index(' void Pump()override{', source_text.index('struct Driver final:'))
            pump_end = source_text.index(' void Idle()override{', pump_start)
            surrender_pump = ''' void Pump()override{
  if(g_MatrixMap->m_Flags&MMFLAG_TRANSITION){events.FocusChanged(true);++warming;assert(warming<=200);return;}
  ++pumps;
  switch(pumps){
  case 1:events.FocusChanged(true);assert(g_MatrixMap->IsPaused());Key(13);assert(!g_MatrixMap->IsPaused());break;
  case 2:Key(27);assert(g_MatrixMap->m_DialogModeName && std::wcscmp(g_MatrixMap->m_DialogModeName,TEMPLATE_DIALOG_MENU)==0);break;
  case 3:Key('S');assert(g_MatrixMap->m_DialogModeHints.size()>1);std::puts("SURRENDER original_menu_confirmation=PASS");break;
  case 4:Key(13);assert(g_ExitState==4);natural_outcome=4;g_FormCur->Takt(25);
   assert(g_MatrixMap->m_DialogModeName && std::wcscmp(g_MatrixMap->m_DialogModeName,TEMPLATE_DIALOG_STATISTICS)==0);
   CGame::SaveResult(&expected);Key(13);assert(g_ExitState==4 && (g_Flags&GFLAG_EXITLOOP));
   std::puts("SURRENDER original_input_exit_and_statistics=PASS");break;
  default:throw std::runtime_error("unexpected extra surrender input pump");
  }
 }
'''
            failure_pump = ''' void Pump()override{
  ++pumps;std::puts("FAILURE injected_native_loop_callback=REACHED");
  throw std::runtime_error("injected native loop callback failure");
 }
'''
            if oom:
                failure_pump = failure_pump.replace('throw std::runtime_error("injected native loop callback failure");', 'throw std::bad_alloc();')
            source_text = source_text[:pump_start] + (failure_pump if failure else surrender_pump) + source_text[pump_end:]
        begin = '   auto* gov=Globals::GovernmentScreen;gov->InitializeLayout();'
        end = '  GlobalsV::CurrentScreenId=previous_screen;'
        assert source_text.count(begin) == source_text.count(end) == 1
        before, rest = source_text.split(begin, 1)
        _, after = rest.split(end, 1)
        scenario_path = ROOT / ('tests/matrixgame_windows_failure_scenario.inc' if failure else 'tests/matrixgame_windows_surrender_scenario.inc' if surrender else 'tests/matrixgame_windows_dispatcher_scenario.inc')
        scenario = scenario_path.read_text(encoding='utf8')
        if save_retry:
            setup = '''EC_Expression::TVarEC* poisoned = nullptr; std::uintptr_t original_value = 0;
for (int si=0; si<pas::list_count(aGalaxy::Galaxy->Scripts); ++si) {
 auto* script=pas::list_at<aScript::TScript>(aGalaxy::Galaxy->Scripts, si);
 if (script->ScriptFileName != u"Script.PC_fem_rangers"_w) continue;
 auto* vars=script->InitCode->LocalVar;
 for(int vi=0;vi<vars->Count;++vi) {auto* cell=EC_Expression::TVarArrayEC_GetItem(vars,vi);
  if(cell->Name == u"quest_item"_w && cell->Kind == EC_Expression::vkDword) poisoned=cell;}
}
assert(poisoned); original_value=poisoned->GetDword();
bool error_dialog_expected=false, error_dialog_acknowledged=false, rejected_tested=false;
'''
            scenario = setup + scenario
            marker = ' if (current->ExitCode) return;\n'
            assert scenario.count(marker) == 1
            scenario = scenario.replace(marker, marker + ''' if (error_dialog_expected && current != gov) {
  auto* target=native_input::CurrentUiTarget(); assert(target);
  error_dialog_acknowledged=true; error_dialog_expected=false;
  target->Key(true,13); target->Key(false,13); return;
 }
''', 1)
            marker = '  auto* player = aPlayer::GetPlayer();\n  history_before = player->PlanetBattleHistory.length();'
            assert scenario.count(marker) == 1
            scenario = scenario.replace(marker, '''  auto* player = aPlayer::GetPlayer();
  if (!rejected_tested) {
   auto* world=aGalaxy::Galaxy; const auto money=player->Money, turn=world->CurrentTurn;
   const auto history=player->PlanetBattleHistory.length(), battles=player->PlanetBattles;
   using Start=void (fGov::TfGov::*)(std::int32_t);
   for (Start start : {&fGov::TfGov::StartPlanetBattleWithoutSupport, &fGov::TfGov::StartPlanetBattleWithReinforcements, &fGov::TfGov::StartPlanetBattleWithBombardment}) {
    poisoned->SetDword(static_cast<std::uintptr_t>(0x123456789abcdef0ULL));
    error_dialog_expected=true; error_dialog_acknowledged=false; (gov->*start)(map_id);
    assert(error_dialog_acknowledged && !error_dialog_expected && gov->PendingTransition==0 && !GR_Main::RobotBattleActive);
    assert(aGalaxy::Galaxy==world && player==aPlayer::GetPlayer() && player->Money==money && world->CurrentTurn==turn);
    assert(player->PlanetBattleHistory.length()==history && player->PlanetBattles==battles && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
   }
   poisoned->SetDword(original_value); rejected_tested=true;
   std::puts("SAVE_RETRY all_three_autosave_failures_return_to_Gov=PASS");
  }
  history_before = player->PlanetBattleHistory.length();''', 1)
            scenario += '\nassert(rejected_tested);\n'
            if classification:
                marker = '   auto* world=aGalaxy::Galaxy; const auto money=player->Money, turn=world->CurrentTurn;'
                assert scenario.count(marker) == 1
                scenario = scenario.replace(marker, marker + '''
   { srhd_awa::platform::e2e5_save_commit_wait::Scope wait;
     assert(aSaveLoad::SaveGameToFile(fSaveManager::TfSaveManager::GetAutoSavePath(), u"as"_w)); }
   const auto save_path=std::filesystem::path(output_root)/"rangers-user/config/save/AutoSave.sav";
   auto read_save=[&]{std::ifstream in(save_path,std::ios::binary); assert(in); return std::string(std::istreambuf_iterator<char>(in),{});};
   const auto original_save=read_save(); assert(!original_save.empty());''', 1)
                marker = '    assert(error_dialog_acknowledged && !error_dialog_expected && gov->PendingTransition==0 && !GR_Main::RobotBattleActive);'
                assert scenario.count(marker) == 1
                scenario = scenario.replace(marker, marker + '\n    assert(read_save()==original_save);', 1)
                scenario = scenario.replace('SAVE_RETRY all_three_autosave_failures_return_to_Gov=PASS', 'CLASSIFICATION_FAILURE old_save_unchanged_all_three_handlers=PASS')
            if worker_failure:
                scenario = scenario.replace('   auto* world=aGalaxy::Galaxy; const auto money=player->Money, turn=world->CurrentTurn;', '''   auto* world=aGalaxy::Galaxy; const auto money=player->Money, turn=world->CurrentTurn;
   { srhd_awa::platform::e2e5_save_commit_wait::Scope wait;
     assert(aSaveLoad::SaveGameToFile(fSaveManager::TfSaveManager::GetAutoSavePath(), u"as"_w)); }
   const auto save_path=std::filesystem::path(output_root)/"rangers-user/config/save/AutoSave.sav";
   auto read_save=[&]{std::ifstream in(save_path,std::ios::binary); assert(in); return std::string(std::istreambuf_iterator<char>(in),{});};
   const auto original_save=read_save(); assert(!original_save.empty());''', 1)
                scenario = scenario.replace('poisoned->SetDword(static_cast<std::uintptr_t>(0x123456789abcdef0ULL));', 'matrixgame_worker_failure_probe::fail_before_replace.store(true);', 1)
                scenario = scenario.replace('    assert(error_dialog_acknowledged && !error_dialog_expected && gov->PendingTransition==0 && !GR_Main::RobotBattleActive);', '    assert(error_dialog_acknowledged && !error_dialog_expected && gov->PendingTransition==0 && !GR_Main::RobotBattleActive);\n    assert(!matrixgame_worker_failure_probe::fail_before_replace.load() && read_save()==original_save);', 1)
                scenario = scenario.replace('SAVE_RETRY all_three_autosave_failures_return_to_Gov=PASS', 'WORKER_FAILURE three_failures_preserve_previous_save=PASS')
                if concurrency:
                    marker = '   const auto original_save=read_save(); assert(!original_save.empty());'
                    assert scenario.count(marker) == 1
                    scenario = scenario.replace(marker, '   auto original_save=read_save(); assert(!original_save.empty());', 1)
                    marker = '   auto original_save=read_save(); assert(!original_save.empty());'
                    concurrent = r'''
   const auto manual_path=pas::concat_wide({GR_Main::GetGameUserDirectory(),u"Save\\ConcurrentManual.sav"});
   assert(aSaveLoad::SaveGameToFile(manual_path,u"concurrent baseline"_w));
   const auto disk_manual=std::filesystem::path(output_root)/"rangers-user/config/save/ConcurrentManual.sav";
   auto read_manual=[&]{std::ifstream in(disk_manual,std::ios::binary); assert(in); return std::string(std::istreambuf_iterator<char>(in),{});};
   const auto original_manual=read_manual(); assert(!original_manual.empty());
   matrixgame_worker_failure_probe::fail_before_replace.store(true);
   bool first_result=true, second_result=false;
   std::thread first([&]{ matrixgame_save_concurrency_probe::role=1;
      first_result=aSaveLoad::SaveGameToFile(manual_path,u"failed concurrent"_w); });
   std::thread second([&]{ matrixgame_save_concurrency_probe::role=2;
      second_result=aSaveLoad::SaveGameToFile(fSaveManager::TfSaveManager::GetAutoSavePath(),u"as"_w); });
   first.join(); second.join();
   aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
   assert(!first_result && second_result && read_manual()==original_manual);
   assert(srhd_awa::platform::e2e4_quicksave::GameSaveCommitted());
   assert(matrixgame_save_concurrency_probe::second_attempted);
   original_save=read_save();
   std::puts("CONCURRENT_SAVE failed_manual_and_successful_auto=PASS");
'''
                    scenario = scenario.replace(marker, marker + '\n' + concurrent, 1)
        if unknown_transition:
            setup = '''EC_Expression::TVarEC* poisoned = nullptr; std::uintptr_t original_value = 0;
for (int si=0; si<pas::list_count(aGalaxy::Galaxy->Scripts); ++si) {
 auto* script=pas::list_at<aScript::TScript>(aGalaxy::Galaxy->Scripts, si);
 if (script->ScriptFileName != u"Script.PC_fem_rangers"_w) continue;
 auto* vars=script->InitCode->LocalVar;
 for(int vi=0;vi<vars->Count;++vi) {auto* cell=EC_Expression::TVarArrayEC_GetItem(vars,vi);
  if(cell->Name == u"quest_item"_w && cell->Kind == EC_Expression::vkDword) poisoned=cell;}
}
assert(poisoned); original_value=poisoned->GetDword();
'''
            scenario = setup + scenario
            marker = '  stage = 2; gov->StartPlanetBattleWithBombardment(map_id);'
            assert scenario.count(marker) == 1
            scenario = scenario.replace(marker, '  poisoned->SetDword(static_cast<std::uintptr_t>(0x123456789abcdef0ULL));\n' + marker, 1)
            scenario = scenario.replace('assert(caught && cancel_sent && battle.pumps == 1);', 'assert(caught && !cancel_sent && battle.pumps == 0);\nassert(poisoned->GetDword() == static_cast<std::uintptr_t>(0x123456789abcdef0ULL));\npoisoned->SetDword(original_value);\nstd::puts("FAILURE unknown_snapshot_rejected=PASS");')
        if missing_map:
            scenario = scenario.replace('bool cancel_sent = false, caught = false;', 'bool cancel_sent = false, caught = false, failure_armed = false;')
            original_start = '  stage = 2; gov->StartPlanetBattleWithBombardment(map_id);'
            assert scenario.count(original_start) == 1
            scenario = scenario.replace(original_start, '  stage = 2; Globals::RobotMapDefinitions[map_index].Map=u"__codex_missing_map__.cmap"_w;\n  std::puts("FAILURE missing_map_fixture=ARMED"); gov->StartPlanetBattleWithBombardment(map_id); failure_armed=true;', 1)
            scenario = scenario.replace('stage == 2 && battle.pumps && current != gov && !cancel_sent', 'stage == 2 && failure_armed && current != gov && !cancel_sent')
            scenario = scenario.replace('assert(caught && cancel_sent && battle.pumps == 1);', 'assert(caught && cancel_sent && battle.pumps == 0);')
        if menu:
            initial_layout = 'Globals::GalaxyScreen->InitializeLayout();\nGlobals::PlanetScreen->InitializeLayout();\nauto* gov = Globals::GovernmentScreen;\ngov->InitializeLayout();\nassert(gov->MainPanel->Screen == gov);'
            assert scenario.count(initial_layout) == 1
            scenario = scenario.replace(initial_layout, 'auto* gov = Globals::GovernmentScreen;', 1)
            scenario = scenario.replace('campaign.Galaxy().CurrentTurn', 'aGalaxy::Galaxy->CurrentTurn')
            marker = 'int stage = 0, history_before = 0'
            assert scenario.count(marker) == 1
            scenario = scenario.replace(marker, 'bool main_clicked=false, start_clicked=false, landing_ordered=false, takeoff_clicked=false;\nint last_travel_turn=-1;\nauto last_key=std::chrono::steady_clock::time_point{};\n' + marker, 1)
            marker = ' if (current->ExitCode) return;\n'
            assert scenario.count(marker) == 1
            menu_driver = ''' using namespace std::chrono_literals;
 std::this_thread::sleep_for(1ms);
 const auto now=std::chrono::steady_clock::now();
 auto* target=native_input::CurrentUiTarget();assert(target);
 if(GlobalsV::CurrentScreenId==GlobalsV::screenLoad && now-last_key>500ms){target->Key(true,27);target->Key(false,27);last_key=now;return;}
 if(GlobalsV::CurrentScreenId==GlobalsV::screenMainMenu && !main_clicked){main_clicked=true;std::puts("MENU_BATTLE original_main_menu=REACHED");Globals::MainMenuScreen->NewGameClicked(nullptr);return;}
 if(GlobalsV::CurrentScreenId==GlobalsV::screenNewGame && !start_clicked){
  auto* ok=pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(Globals::NewGameScreen->GetByName(u"Ok"sv));
  if(!ok->Disabled){start_clicked=true;std::puts("MENU_BATTLE original_new_game=START");Globals::NewGameScreen->ApplyClicked(nullptr);}return;
 }
 if(GlobalsV::CurrentScreenId==GlobalsV::screenIntroduction && aGalaxy::Galaxy && !Globals::NewGameGenerationThread && now-last_key>500ms){
  target->Key(true,13);target->Key(false,13);last_key=now;return;
 }
 if(GlobalsV::CurrentScreenId==GlobalsV::screenRuinsTalk && current==Globals::RuinsTalkScreen && !takeoff_clicked){
  Globals::RuinsTalkScreen->StationPanel->HangarClicked(nullptr);
  if(GlobalsV::RequestedScreenId==GlobalsV::screenHangar)std::puts("MENU_BATTLE original_station_to_hangar=PASS");
  return;
 }
 if(GlobalsV::CurrentScreenId==GlobalsV::screenHangar && current==Globals::HangarScreen && !takeoff_clicked){
  Globals::HangarScreen->TakeOffClicked(nullptr);
  if(Globals::HangarScreen->TakeOffPending){takeoff_clicked=true;std::puts("MENU_BATTLE original_hangar_takeoff=PASS");}
  return;
 }
 if(GlobalsV::CurrentScreenId==GlobalsV::screenStarMap && current==Globals::StarMapScreen && takeoff_clicked && Globals::StarMapScreen->Mode==fStarMap::smmOrders){
  auto* player=aPlayer::GetPlayer();assert(player && player->HomePlanet && player->CurrentStar==player->HomePlanet->CurrentStar);
  if(!landing_ordered){player->OrderLanding(player->HomePlanet,false);landing_ordered=true;std::puts("MENU_BATTLE original_landing_order=PASS");}
  if(last_travel_turn!=aGalaxy::Galaxy->CurrentTurn){last_travel_turn=aGalaxy::Galaxy->CurrentTurn;std::printf("MENU_BATTLE travel_turn=%d\\n",last_travel_turn);Globals::StarMapScreen->EndTurnClicked(nullptr);}
  return;
 }
'''
            scenario = scenario.replace(marker, marker + menu_driver, 1)
            initial = 'GlobalsV::RequestedScreenId = GlobalsV::screenPlanet;\nGlobals::RunMainScreenStateLoop();'
            assert scenario.count(initial) == 1
            scenario = scenario.replace(initial, 'GlobalsV::PostLoadScreenId=GlobalsV::screenMainMenu; GlobalsV::RequestedScreenId=GlobalsV::screenLoad; Globals::ScreenLoadMode=0;\nGlobals::RunMainScreenStateLoop();\nassert(main_clicked && start_clicked);\nstd::puts("MENU_BATTLE original_menu_generation_to_battles=PASS");', 1)
            scenario += '\nm::ShutdownRangersWorld(scripts);MessageText::QuestMessages=nullptr;pas::free(messages);\n'
        source_text = before + scenario + '\n  }\n' + end + after
        source = out / 'campaign_frun_dispatcher.cpp'
        source.write_text(source_text, encoding='utf8')
        (out / 'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="' + sha(source) + '";\n', encoding='utf8')
        entry = next(x for x in old['commands'] if x['label'] == 'compile-campaign_frun')

        def compile_source(label, source_file, obj):
            command = entry['command'][:]
            command.insert(1, '/I' + str(out))
            if worker_failure or classification: command.insert(1, '/I' + str(ROOT / 'tests'))
            command[command.index('/c') + 1] = str(source_file)
            command = [('/Fo' + str(obj)) if x.startswith('/Fo') else x for x in command]
            run(label, command)

        obj = out / 'campaign_frun_dispatcher.obj'
        input_obj = out / 'rangers_native_input.obj'
        compile_source('compile-scenario', source, obj)
        compile_source('compile-input', ROOT / 'port/matrixgame/native/rangers_native_input.cpp', input_obj)
        old_obj = prior / 'campaign_frun.obj'
        items = (prior / 'link.rsp').read_text(encoding='utf8').splitlines()
        assert sum(line.strip('"') == str(old_obj) for line in items) == 1
        replacements = {str(old_obj): obj}
        if savefix_regression:
            old_galaxy = str(BASE / 'rangers-windows-2/objects/original/aGalaxy.obj')
            assert sum(line.strip('"') == old_galaxy for line in items) == 1
            replacements[old_galaxy] = BASE / 'item-roots-fixed-1/aGalaxy-fixed.obj'
            assert replacements[old_galaxy].is_file()
        if snapshot_regression:
            from matrixgame_save_buffer_ownership import apply
            from matrixgame_snapshot_preparation import apply as apply_snapshot
            saver = BASE / 'rangers-windows-2/source/aSaveLoad.cpp'
            fixed_saver = out / 'aSaveLoad.cpp'
            from matrixgame_save_commit_wait import apply as apply_wait
            saver_text = apply_wait(apply_snapshot(apply(saver.read_text(encoding='utf8'))))
            if concurrency:
                entry_marker = '        srhd_awa::platform::e2e5_save_commit_wait::SaveCall save_call;'
                wait = '                    SaveWriter->WaitForIdle(WindowsSdk::INFINITE);'
                assert saver_text.count(entry_marker) == saver_text.count(wait) == 1
                saver_text = '#include "matrixgame_save_concurrency_probe.hpp"\n' + saver_text.replace(entry_marker, '        matrixgame_save_concurrency_probe::BeforeEntry();\n' + entry_marker, 1).replace(wait, wait + '\n                    matrixgame_save_concurrency_probe::AfterFirstWait();', 1)
            if worker_failure:
                marker = '                const auto replacement = srhd_awa::platform::e2e4_quicksave::ReplaceGameSavePath('
                assert saver_text.count(marker) == 1
                saver_text = '#include "matrixgame_worker_failure_probe.hpp"\n' + saver_text.replace(marker, '                if (matrixgame_worker_failure_probe::fail_before_replace.exchange(false))\n                    pas::raise(pas::make_exception<pas::Exception>("injected worker replacement failure"_a));\n' + marker, 1)
            fixed_saver.write_text(saver_text, encoding='utf8')
            saver_obj = out / 'aSaveLoad.obj'
            flags = json.loads((BASE / 'rangers-windows-2/result.json').read_text(encoding='utf8'))['flags']
            run('compile-fixed-saver', [entry['command'][0], '/I' + str(ROOT / 'tests'), *flags, '/c', fixed_saver, '/Fo' + str(saver_obj)])
            old_saver = str(BASE / 'rangers-windows-2/objects/original/aSaveLoad.obj')
            assert sum(line.strip('"') == old_saver for line in items) == 1
            replacements[old_saver] = saver_obj
        if save_retry:
            from matrixgame_gov_autosave_guard import apply as apply_gov_guard
            gov_source = out / 'fGov.cpp'
            gov_source.write_text(apply_gov_guard('fGov.cpp', (BASE / 'rangers-windows-2/source/fGov.cpp').read_text(encoding='utf8')), encoding='utf8')
            gov_obj = out / 'fGov.obj'
            run('compile-fixed-gov', [entry['command'][0], *flags, '/c', gov_source, '/Fo' + str(gov_obj)])
            old_gov = str(BASE / 'rangers-windows-2/objects/original/fGov.obj')
            assert sum(line.strip('"') == old_gov for line in items) == 1
            replacements[old_gov] = gov_obj
        if missing_map or fixed_regression:
            native_commands = json.loads((BASE / 'native-windows-run/compile_commands.json').read_text(encoding='utf8'))
            native = next(item for item in native_commands if item['file'].endswith('native_rangers_interface.cpp'))
            native_command = native['command'].split()
            boundary_obj = out / 'native_rangers_interface_fixed.obj'
            native_command = [('/Fo' + str(boundary_obj)) if item.startswith('/Fo') else
                              ('/Fd' + str(out / 'native_interface.pdb')) if item.startswith('/Fd') else item for item in native_command]
            run('compile-fixed-boundary', native_command)
            items.insert(0, '"' + str(boundary_obj) + '"')
        if missing_map:
            sdk_command = next(x for x in old['commands'] if x['label'] == 'compile-campaign_sdk_main')['command'][:]
            sdk_original = Path(sdk_command[sdk_command.index('/c') + 1])
            sdk_text = sdk_original.read_text(encoding='utf8')
            main_anchor = 'int main(int argc,char** argv){'
            assert sdk_text.count(main_anchor) == 1
            terminate_hook = '''std::set_terminate([]{
 try {if(auto error=std::current_exception())std::rethrow_exception(error);}
 catch(const Base::CException& e){std::printf("MISSING_MAP_TERMINATE_EXCEPTION type=Base::CException info=%s\\n",utils::from_wstring(e.Info()).c_str());}
 catch(const std::exception& e){std::printf("MISSING_MAP_TERMINATE_EXCEPTION type=%s what=%s\\n",typeid(e).name(),e.what());}
 catch(...){std::puts("MISSING_MAP_TERMINATE_EXCEPTION type=non_std");}
 void* stack[40]{};auto count=CaptureStackBackTrace(0,40,stack,nullptr);
 std::printf("MISSING_MAP_TERMINATE image=%p:",static_cast<void*>(GetModuleHandleW(nullptr)));
 for(USHORT i=0;i<count;++i)std::printf(" %p",stack[i]);std::puts("");std::fflush(stdout);
 std::_Exit(86);
});'''
            sdk_text = '#include <exception>\n#include <cstdlib>\n#include <typeinfo>\n#include "CException.hpp"\n#include "utils.hpp"\n' + sdk_text.replace(main_anchor, main_anchor + terminate_hook, 1)
            sdk_source = out / 'campaign_sdk_trace.cpp'; sdk_source.write_text(sdk_text, encoding='utf8')
            sdk_obj = out / 'campaign_sdk_trace.obj'
            sdk_command[sdk_command.index('/c') + 1] = str(sdk_source)
            sdk_command = [('/Fo' + str(sdk_obj)) if x.startswith('/Fo') else x for x in sdk_command]
            run('compile-sdk-trace', sdk_command)
            old_sdk = str(prior / 'campaign_sdk_main.obj')
            assert sum(line.strip('"') == old_sdk for line in items) == 1
            replacements[old_sdk] = sdk_obj
        rsp = out / 'link.rsp'
        rsp.write_text('\n'.join('"' + str(replacements[x.strip('"')]) + '"' if x.strip('"') in replacements else x for x in items) + '\n"' + str(input_obj) + '"\n', encoding='utf8')
        link = next(x for x in old['commands'] if x['label'] == 'link')['command'][:]
        binary = out / 'gov-frun-dispatcher.exe'
        link = [('@' + str(rsp)) if x.startswith('@') else ('/Fe' + str(binary)) if x.startswith('/Fe') else x for x in link]
        run('link', link)
        result.update(status='PASS; reference linked; runtime NOT RUN', source_sha256=sha(source), binary_sha256=sha(binary), scenario_sha256=sha(scenario_path))
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

if __name__ == '__main__':
    main()

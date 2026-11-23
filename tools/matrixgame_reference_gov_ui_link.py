"""Link a menu-to-Gov reference that drives the original quest choices."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from matrixgame_windows_host_env import environment

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
OLD = BASE / 'gov-frun-menu-link-2'
two_battles = sys.argv[1:] == ['--two-battles']
if sys.argv[1:] and not two_battles:
    raise RuntimeError('unknown arguments')
OUT = BASE / ('gov-frun-menu-two-ui-quest-link-2' if two_battles else 'gov-frun-menu-ui-quest-link-4')
if OUT.exists():
    raise RuntimeError('output already exists')
OUT.mkdir()
result = {'status': 'INCOMPLETE', 'commands': []}
env = environment(OUT)

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(label, command):
    command = list(map(str, command))
    process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
    (OUT / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf-8')
    result['commands'].append({'label': label, 'command': command, 'exit': process.returncode})
    print(label, process.returncode, flush=True)
    if process.returncode:
        raise RuntimeError('see ' + label + '.log')

try:
    prior = json.loads((OLD / 'result.json').read_text(encoding='utf-8'))
    source = (OLD / 'campaign_frun_dispatcher.cpp').read_text(encoding='utf-8')
    anchor = '#include "units/fGov.hpp"'
    assert source.count(anchor) >= 1
    source = source.replace(anchor, anchor + '\n#include "types/fTalk.hpp"\n#include "types/GI_PanelScrollBar.hpp"\n#include "e2e5_pointer_payload.hpp"', 1)
    start = source.index('int map_index = -1;')
    end = source.index('RangersCallerPresentation caller_presentation', start)
    source = source[:start] + 'int map_id = -1;\n' + source[end:]
    marker = 'int stage = 0, history_before = 0'
    assert source.count(marker) == 1
    fixture = (ROOT / 'tests/matrixgame_windows_gov_ui_quest.inc').read_text(encoding='utf-8')
    source = source.replace(marker, fixture + '\nint quest_phase=0; auto quest_started=std::chrono::steady_clock::time_point{};\n' + marker, 1)
    start = source.index(' if ((stage == 1 || stage == 4) && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment')
    end = source.index(' if ((stage == 2 || stage == 5)', start)
    driver = ''' if (stage == 1 && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment && current == gov) {
  if(quest_started==std::chrono::steady_clock::time_point{}) quest_started=std::chrono::steady_clock::now();
  if(std::chrono::steady_clock::now()-quest_started>std::chrono::seconds(30)) {
   auto* p=aPlayer::GetPlayer();auto* star=p->CurrentStar;
   std::printf("UI_QUEST no_visible_choice phase=%d turn=%d last_battle=%d last_dominator=%d last_liberation=%d history=%d forced_quest=%d decline=%d\\n",quest_phase,aGalaxy::Galaxy->CurrentTurn,p->LastPlanetBattleTurn,star->LastDominatorPresenceTurn,star->LastLiberationRewardsTurn,int(p->PlanetBattleHistory.length()),GlobalsV::ForcedPlanetQuestId,int(p->DeclinePlanetBattleOffers));
   stage=6; gov->ReturnToPlanet(0); return;
  }
  if(quest_phase==0 && click_gov_choice(pas::bind_method<&fGov::TfGov::RequestQuest>(gov),nullptr)) {
   quest_phase=1;std::puts("UI_QUEST requested_through_mouse=PASS");return;
  }
  if(quest_phase==1 && click_gov_choice(pas::bind_method<&fGov::TfGov::ShowPlanetBattleSupport>(gov),&map_id)) {
   quest_phase=2;std::printf("UI_QUEST offered_map=%d accepted_through_mouse=PASS\\n",map_id);return;
  }
  if(quest_phase==2) {
   auto* player=aPlayer::GetPlayer();history_before=player->PlanetBattleHistory.length();money_before=player->Money;
   turn_before=aGalaxy::Galaxy->CurrentTurn;player_id=player->Id;planet_id=player->CurrentPlanet->Id;battles_before=player->PlanetBattles;
   campaign_build_commands=0;campaign_order_polls=0;campaign_design_selected=false;campaign_last_squad_time=-30000;
   if(!battle.current) {battle.current=std::make_unique<Driver>();GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness,GR_Main::DisplayContrast);CaptureCallerGammaBeforeBattle();}
   if(click_gov_choice(pas::bind_method<&fGov::TfGov::StartPlanetBattleWithBombardment>(gov),nullptr)) {
    stage=2;quest_phase=3;std::puts("UI_QUEST support_mouse_queued");
   }
   return;
  }
  return;
 }
'''
    source = source[:start] + driver + source[end:]
    marker = ' if ((stage == 2 || stage == 5) && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment'
    assert source.count(marker) == 1
    source = source.replace(marker, ''' if(stage==2 && GlobalsV::CurrentScreenId==GlobalsV::screenGovernment && current==gov &&
    !GR_Main::RobotBattleActive && gov->PendingTransition==0 &&
    aPlayer::GetPlayer()->PlanetBattleHistory.length()==history_before &&
    std::chrono::steady_clock::now()-quest_started>std::chrono::seconds(30)) {
  std::puts("UI_QUEST no_visible_choice phase=3");stage=6;gov->ReturnToPlanet(0);return;
 }
''' + marker, 1)
    if two_battles:
        source = source.replace('int quest_phase=0; auto quest_started=', 'int completed_ui_battles=0; int quest_phase=0; auto quest_started=', 1)
        source = source.replace('if (stage == 1 && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment',
                                'if ((stage == 1 || stage == 4) && GlobalsV::CurrentScreenId == GlobalsV::screenGovernment', 1)
        source = source.replace('stage=2;quest_phase=3;std::puts("UI_QUEST support_mouse_queued");',
                                'stage=stage==1?2:5;quest_phase=3;std::puts("UI_QUEST support_mouse_queued");', 1)
        source = source.replace('if(stage==2 && GlobalsV::CurrentScreenId', 'if((stage==2 || stage==5) && GlobalsV::CurrentScreenId', 1)
        source = source.replace('(stage == 0 || stage == 3) && GlobalsV::CurrentScreenId',
                                'stage == 0 && GlobalsV::CurrentScreenId', 1)
        marker = ' if (stage == 1 && GlobalsV::CurrentScreenId' if ' if (stage == 1 && GlobalsV::CurrentScreenId' in source else ' if ((stage == 1 || stage == 4) && GlobalsV::CurrentScreenId'
        assert source.count(marker) == 1
        turns = ''' if(stage==3 && GlobalsV::CurrentScreenId==GlobalsV::screenPlanet && current==Globals::PlanetScreen) {
  const auto turn=aGalaxy::Galaxy->CurrentTurn;
  if(wait_started==std::chrono::steady_clock::time_point{}) wait_started=std::chrono::steady_clock::now();
  if(std::chrono::steady_clock::now()-wait_started>std::chrono::seconds(180)) {
   std::printf("UI_QUEST turn_wait_timeout turn=%d\\n",turn);stage=6;return;
  }
  if(turn-aPlayer::GetPlayer()->LastPlanetBattleTurn>=135) {
   Globals::PlanetScreen->PlanetPanel->GovernmentClicked(nullptr);
   if(GlobalsV::RequestedScreenId==GlobalsV::screenGovernment) {
    quest_phase=0;quest_started={};stage=4;std::printf("UI_QUEST cooldown_elapsed turn=%d\\n",turn);
   }
   return;
  }
  if(std::chrono::steady_clock::now()-last_turn_click>std::chrono::milliseconds(50)) {
   last_turn_click=std::chrono::steady_clock::now();
   if(turn!=last_turn_logged && turn%10==0){last_turn_logged=turn;std::printf("UI_QUEST waiting_turn=%d\\n",turn);}
   Globals::PlanetScreen->EndTurnClicked(nullptr);
  }
  return;
 }
'''
        source = source.replace(marker, turns + marker, 1)
        source = source.replace('int quest_phase=0; auto quest_started=std::chrono::steady_clock::time_point{};',
                                'int quest_phase=0; auto quest_started=std::chrono::steady_clock::time_point{}; auto wait_started=quest_started,last_turn_click=quest_started; int last_turn_logged=-1;', 1)
        source = source.replace('  stage = stage == 2 ? 3 : 6;',
                                '  ++completed_ui_battles; battle.current.reset(); stage = stage == 2 ? 3 : 6;', 1)
        source = source.replace('  std::puts("DISPATCH both_battles_returned_to_planet=PASS");',
                                '  std::puts("UI_QUEST session_returned_to_planet"); if(completed_ui_battles==2) std::puts("DISPATCH both_battles_returned_to_planet=PASS");', 1)
    else:
        source = source.replace('stage = stage == 2 ? 3 : 6;', 'stage = 6;', 1)
        source = source.replace('DISPATCH both_battles_returned_to_planet=PASS', 'UI_QUEST returned_to_planet=PASS', 1)
    source = source.replace('MENU_BATTLE original_menu_generation_to_battles=PASS', 'UI_QUEST menu_session_finished', 1)
    generated = OUT / 'campaign_frun_ui_quest.cpp'
    generated.write_text(source, encoding='utf-8')
    (OUT / 'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="' + sha(generated) + '";\n', encoding='utf-8')
    compile_command = next(x['command'] for x in prior['commands'] if x['label'] == 'compile-scenario')[:]
    compile_command.insert(1, '/I' + str(ROOT / 'tests'))
    compile_command[compile_command.index('/c') + 1] = str(generated)
    obj = OUT / 'campaign_frun_ui_quest.obj'
    compile_command = [('/Fo' + str(obj)) if item.startswith('/Fo') else item for item in compile_command]
    run('compile', compile_command)
    replacements = {
        str(OLD / 'campaign_frun_dispatcher.obj'): obj,
        str(BASE / 'rangers-windows-2/objects/original/aGalaxy.obj'): BASE / 'item-roots-fixed-1/aGalaxy-fixed.obj',
        str(BASE / 'rangers-windows-2/objects/original/aSaveLoad.obj'): BASE / 'gov-frun-classification-save-link-1/aSaveLoad.obj',
        str(BASE / 'rangers-windows-2/objects/original/fGov.obj'): BASE / 'gov-frun-classification-save-link-1/fGov.obj',
    }
    items = (OLD / 'link.rsp').read_text(encoding='utf-8').splitlines()
    for old, new in replacements.items():
        assert new.is_file() and sum(item.strip('"') == old for item in items) == 1, old
    rsp = OUT / 'link.rsp'
    fixed_boundary = BASE / 'gov-frun-classification-save-link-1/native_rangers_interface_fixed.obj'
    rsp.write_text('"' + str(fixed_boundary) + '"\n' + '\n'.join('"' + str(replacements[x.strip('"')]) + '"' if x.strip('"') in replacements else x for x in items) + '\n', encoding='utf-8')
    link = next(x['command'] for x in prior['commands'] if x['label'] == 'link')[:]
    binary = OUT / 'gov-frun-dispatcher.exe'
    link = [('@' + str(rsp)) if item.startswith('@') else ('/Fe' + str(binary)) if item.startswith('/Fe') else item for item in link]
    run('link', link)
    result.update(status='PASS; Windows reference linked; runtime NOT RUN', binary_sha256=sha(binary), source_sha256=sha(generated))
finally:
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')

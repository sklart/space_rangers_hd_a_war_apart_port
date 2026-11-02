"""Check original AI attack, actual hostile HP reductions and preserved SDK inputs."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e588-hosted-battle'

def fields(line):return dict(p.split('=',1) for p in line.split()[1:])

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 paths=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);record=read(paths[0]);assert record['status'].startswith('PASS')
 assert all(c['exit']==0 for c in record['commands']) and '/UNDEBUG' in record['flags'] and '/DNDEBUG' not in record['flags']
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(record[key])
 assert sha(BASE/'hosted-map.exe')==record['binary_sha256']
 prefix='#include "matrixgame_battle_probe.hpp"\n';text=(BASE/'observed_battle_weapon.cpp').read_text(encoding='utf-8');assert text.startswith(prefix);text=text[len(prefix):]
 for hook in ('            auto battle_hit=BeginBattleHit(hiti,w->m_Owner,int(w->m_Type));\n','\n            EndBattleHit(battle_hit,odead);'):
  assert text.count(hook)==2;text=text.replace(hook,'')
 original=ROOT/'build/matrixgame/e2e583-robot-orders/native-build/native/source/MatrixGame/src/Effects/MatrixEffectWeapon.cpp'
 assert text==original.read_text(encoding='utf-8')
 sdk=ROOT/'build/matrixgame/e2e586-interactive-diagnostic/sdk';matches=list((sdk/'source').rglob('MatrixEffectWeapon.cpp'));assert len(matches)==1 and text==matches[0].read_text(encoding='utf-8')
 sessions=[];current={}
 for line in record['stdout'].splitlines():
  if line.startswith('BATTLE_INVENTORY '):assert not current;current={'inventory':fields(line),'hits':[]}
  elif line.startswith('BATTLE_COMMAND '):current['command']=fields(line)
  elif line.startswith('BATTLE_HIT '):current['hits'].append(fields(line))
  elif line.startswith('BATTLE_RESULT '):current['result']=fields(line)
  elif line.startswith('HOSTED_BATTLE_LOOP '):
   current['loop']=fields(line);sessions.append(current);current={}
 assert not current and len(sessions)==2 and sessions[0]==sessions[1]
 for session in sessions:
  inventory,command,hits,result,loop=[session[k] for k in ('inventory','command','hits','result','loop')]
  assert int(inventory['players'])>0 and int(inventory['hostile_combatants'])>0 and math.isfinite(float(inventory['nearest_distance']))
  assert float(inventory['nearest_distance'])>0 and command['original_PGOrderAttack']=='EXECUTED'
  assert int(command['target_side'])>0 and int(command['target_side'])!=1
  assert len(hits)==int(result['hit_calls'])>0
  decreases=0
  for hit in hits:
   before,after=float(hit['hp_before']),float(hit['hp_after']);assert math.isfinite(before) and math.isfinite(after)
   assert int(hit['side'])>0 and int(hit['side'])!=1
   assert int(command['time'])<=int(hit['time'])<=int(command['time'])+int(result['elapsed'])
   decreases+=int(hit['hp_after_known'])!=0 and after<before
  assert decreases==int(result['HP_decreases'])>0 and result['no_position_HP_weapon_injection']=='PASS'
  extra=int(loop['extra_form_Takt25']);assert 0<extra<=20*200
  assert (int(loop['draws']),int(loop['loop_elapsed']))==(208,5125)
  assert int(loop['map_elapsed'])==5125+extra*25
  assert int(command['time'])+int(result['elapsed'])==int(loop['map_elapsed'])
  assert all(loop[k]=='PASS' for k in ('input_pause_menu_camera','focus_release_recreate','host_callbacks_live'))
 assert record['stdout'].count('BATTLE original_attack_navigation_weapon_virtual_Damage_HP=PASS')==2
 assert record['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and record['stdout'].count('readback_present=PASS')==8
 assert record['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==2
 images={};digests=[]
 for index in range(8):
  path=BASE/f'runtime/output/map-frame-{index}.png';w,h,rgb=decode(path.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  count=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert count>=64;digest=hashlib.sha256(rgb).hexdigest();digests.append(digest)
  images[str(path.relative_to(ROOT))]={'sha256':sha(path),'rgb_sha256':digest,'distinct_rgb':count}
 assert len(set(digests[:4]))==len(set(digests[4:]))==4
 journals={}
 for path in (BASE/'runtime/output/journals').rglob('run.jsonl'):
  rows=[json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
  assert [r['seq'] for r in rows]==list(range(1,len(rows)+1))
  assert all(r['source_sha256']==record['composition_source_sha256'] and r['build_id']=='E588-Windows-HAL-hosted-battle' for r in rows)
  assert rows[0]['event']=='journal_open' and rows[-1]['event']=='journal_close'
  assert any(r['event']=='ok' and r['operation']=='B.CGame::Init' for r in rows)
  assert not any(r['event'] in ('fail','error') for r in rows)
  journals[str(path.relative_to(ROOT))]={'sha256':sha(path),'rows':len(rows),'full_original_Init':True,'closed':True}
 assert len(journals)==2
 previous=read(ROOT/'reports/matrixgame-e2e586-interactive-diagnostic-final-evidence.json')
 check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 for package in previous['packages'].values():assert sha(ROOT/package['path'])==package['sha256']
 metadata=read(sdk/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk/'source');check_hashes(metadata['rangers_host']['archives_sha256'])
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint']
 assert sha(Path(primary['path']))==primary['sha256'] and sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
  assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 extra=[ROOT/'tests/matrixgame_battle_probe.hpp',ROOT/'tools/matrixgame_hosted_battle_checks.py',original,Path(__file__).resolve()]
 result={'status':'PASS; ORIGINAL AI ATTACK AND HOSTILE ROBOT HP DAMAGE IN TWO WINDOWS HAL SESSIONS; ACCELERATED ORIGINAL FORM TAKT; NO SWITCH',
  'execution':record,'sessions':sessions,'original_weapon_body_restored_exactly':True,'images':images,'journals':journals,
  'extra_inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in extra},'clean_upstream_pins':pins,
  'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],
  'preserved_E586_packages':previous['packages'],'production_changes':False,'new_NRO':False,
  'ASan_UBSan':'NOT RERUN; TEST OBSERVATION ONLY','ARM64_QEMU':'NOT RERUN; TEST OBSERVATION ONLY','Switch':'NOT RUN',
  'target_identity_between_hits':'NOT RECORDED; OBSERVER ACCEPTS HOSTILE COMBATANTS HIT BY CHOSEN ATTACKER',
  'remaining':['full native/deko Init and visible map frames','Switch image/audio/HID/F9/focus/recovery','battle outcome and return to owning game'],'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(sessions)
if __name__=='__main__':main()

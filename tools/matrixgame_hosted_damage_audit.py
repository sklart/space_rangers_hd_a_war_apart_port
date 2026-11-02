"""Audit actual virtual Damage state transitions without changing game state."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e587-hosted-damage'

def fields(line):
 return dict(part.split('=',1) for part in line.split()[1:])

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 paths=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True)
 record=read(paths[0]);assert record['status'].startswith('PASS')
 assert all(c['exit']==0 for c in record['commands']) and '/UNDEBUG' in record['flags'] and '/DNDEBUG' not in record['flags']
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(record[key])
 assert sha(BASE/'hosted-map.exe')==record['binary_sha256']
 native=ROOT/'build/matrixgame/e2e583-robot-orders/native-build/native/source/MatrixGame/src'
 sdk=ROOT/'build/matrixgame/e2e586-interactive-diagnostic/sdk/source'
 originals={}
 for generated,relative,prefix,hooks in (
  ('observed_weapon_damage_guarded.cpp','Effects/MatrixEffectWeapon.cpp','#include "matrixgame_firing_probe.hpp"\n#include "matrixgame_damage_probe.hpp"\n',(
   '\n    ObserveOriginalFire(*this,m_Owner);','\n    ObserveOriginalHit(*w,w->m_Owner,hiti,flags);',
   '            auto observed_damage=BeginOriginalDamage(hiti,w->m_Owner,int(w->m_Type));\n',
   '\n            ObserveOriginalDamage(observed_damage,odead);')),
  ('observed_map_object.cpp','MatrixObject.cpp','#include "matrixgame_damage_probe.hpp"\n',(
   '            ObserveOriginalBurnState(this,false,IsAblaze(),GetAblazeTTL());\n',
   '\n            ObserveOriginalBurnState(this,true,IsAblaze(),GetAblazeTTL());'))):
  text=(BASE/generated).read_text(encoding='utf-8');assert text.startswith(prefix);text=text[len(prefix):]
  for hook in hooks:
   expected=2 if 'observed_damage' in hook else 1
   assert text.count(hook)==expected;text=text.replace(hook,'')
  original=native/relative;assert text==original.read_text(encoding='utf-8')
  matches=list(sdk.rglob(Path(relative).name));assert len(matches)==1 and text==matches[0].read_text(encoding='utf-8')
  originals[relative]={'native_sha256':sha(original),'SDK_sha256':sha(matches[0]),'body_restored_exactly':True}
 sessions=[];events=[]
 for line in record['stdout'].splitlines():
  if line.startswith('DAMAGE_EVENT '):
   e=fields(line);assert int(e['index'])==len(events)
   flag=lambda name:int(e[name])!=0
   health=flag('health_known') and float(e['hp_after'])<float(e['hp_before'])
   ignition=flag('burn_known') and not flag('ablaze_before') and flag('ablaze_after')
   ttl=flag('burn_known') and flag('ablaze_after') and int(e['ttl_after'])>int(e['ttl_before'])
   gone=not flag('exists_after')
   changed=health or ignition or ttl or gone or e['behaviour_before']!=e['behaviour_after']
   assert flag('state_changed')==changed
   events.append({'values':e,'health_decrease':health,'ignition':ignition,'ttl_increase':ttl,'removed':gone,'changed':changed})
  elif line.startswith('DAMAGE_RESULT '):
   summary=fields(line)
   assert int(summary['calls'])==len(events)>0
   for key,event_key in (('state_changes','changed'),('health_decreases','health_decrease'),('ignitions','ignition'),('ttl_increases','ttl_increase'),('removals','removed')):
    assert int(summary[key])==sum(e[event_key] for e in events)
   assert int(summary['state_changes'])>0
   assert all(summary[k]=='PASS' for k in ('actual_virtual_Damage','no_HP_TTL_position_injection','guarded_core_lifetime'))
   sessions.append({'events':events,'summary':summary});events=[]
 assert not events and len(sessions)==2 and sessions[0]==sessions[1]
 assert all(int(s['summary']['calls'])==7 and int(s['summary']['ignitions'])==2 and int(s['summary']['health_decreases'])==0 for s in sessions)
 assert record['stdout'].count('original_callbacks=PASS counter_consistency=PASS stop_effective=PASS live_robot=PASS')==2
 assert record['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and record['stdout'].count('readback_present=PASS')==8
 assert record['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==2
 images={};digests=[]
 for index in range(8):
  p=BASE/f'runtime/output/map-frame-{index}.png';w,h,rgb=decode(p.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  count=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert count>=64
  digest=hashlib.sha256(rgb).hexdigest();digests.append(digest);images[str(p.relative_to(ROOT))]={'sha256':sha(p),'rgb_sha256':digest,'distinct_rgb':count}
 assert len(set(digests[:4]))==len(set(digests[4:]))==4
 previous=read(ROOT/'reports/matrixgame-e2e586-interactive-diagnostic-final-evidence.json')
 check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 packages=previous['packages']
 for package in packages.values():assert sha(ROOT/package['path'])==package['sha256']
 metadata=read(sdk.parent/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk);check_hashes(metadata['rangers_host']['archives_sha256'])
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint']
 assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
  assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 extra=[ROOT/'tests/matrixgame_damage_probe.hpp',ROOT/'tools/matrixgame_hosted_damage_checks.py',Path(__file__).resolve()]
 result={'status':'PASS; ORIGINAL VIRTUAL DAMAGE IGNITES TWO MAP OBJECTS IN EACH OF TWO WINDOWS HAL LOOPS; NO SWITCH',
  'execution':record,'damage_sessions':sessions,'original_bodies':originals,'images':images,
  'extra_inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in extra},'clean_upstream_pins':pins,
  'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],
  'preserved_E586_packages':packages,'new_NRO':False,'production_changes':False,
  'ASan_UBSan':'NOT RERUN; TEST OBSERVATION ONLY','ARM64_QEMU':'NOT RERUN; TEST OBSERVATION ONLY',
  'Switch':'NOT RUN','enemy_HP_decrease':'NOT PROVEN','full_battle':'NOT PROVEN',
  'remaining':['full native/deko Init and visible map frames','Switch image/audio/HID/F9/focus/recovery','hostile HP damage, battle outcome and return to owning game'],'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print([s['summary'] for s in sessions])
if __name__=='__main__':main()

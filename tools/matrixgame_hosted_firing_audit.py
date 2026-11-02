"""Validate observed original weapon calls, unchanged body, snapshots and SDK."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e585-hosted-firing'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 paths=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);record=next(read(p) for p in paths if read(p)['status'].startswith('PASS'))
 assert all(c['exit']==0 for c in record['commands']) and '/UNDEBUG' in record['flags'] and '/DNDEBUG' not in record['flags']
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(record[key])
 assert sha(BASE/'hosted-map.exe')==record['binary_sha256']
 original=ROOT/'build/matrixgame/e2e583-robot-orders/native-build/native/source/MatrixGame/src/Effects/MatrixEffectWeapon.cpp'
 text=(BASE/'observed_weapon.cpp').read_text(encoding='utf-8');prefix='#include "matrixgame_firing_probe.hpp"\n';assert text.startswith(prefix);restored=text[len(prefix):]
 for hook in ('\n    ObserveOriginalFire(*this,m_Owner);','\n    ObserveOriginalHit(*w,w->m_Owner,hiti,flags);'):
  assert restored.count(hook)==1;restored=restored.replace(hook,'',1)
 assert restored==original.read_text(encoding='utf-8')
 commands=re.findall(r'^FIRING_COMMAND time=(\d+) range=([\d.]+) key=(\d+) original_arcade_EngineSink_KA_FIRE=EXECUTED$',record['stdout'],re.M)
 results=re.findall(r'^FIRING_RESULT elapsed=(\d+) held=(\d+) settle=(\d+) weapons=(\d+) shots=(\d+) hit_callbacks=(\d+) object_hits=(\d+) landscape_hits=(\d+) last_fire=(\d+) stop=(\d+) original_callbacks=PASS counter_consistency=PASS stop_effective=PASS live_robot=PASS$',record['stdout'],re.M)
 weapon_rows=re.findall(r'^FIRING_WEAPON type=(\d+) first=(\d+) last=(\d+) events=(\d+)$',record['stdout'],re.M);assert len(commands)==len(results)==2
 observations=[];offset=0
 for command,row in zip(commands,results):
  elapsed,held,settle,weapons,shots,hits,objects,land,last,stop=map(int,row);start,reach,key=int(command[0]),float(command[1]),int(command[2]);assert (elapsed,held,settle,start,stop)==(5000,4000,1000,125,4125)
  assert key and reach>40 and shots>0 and hits>0 and weapons>0 and objects+land>0 and objects+land<=hits and last<=stop+500
  counts=[tuple(map(int,r)) for r in weapon_rows[offset:offset+weapons]];offset+=weapons
  assert len(counts)==weapons and all(last-first+1==events for _,first,last,events in counts) and sum(r[3] for r in counts)==shots
  observations.append({'elapsed_ms':elapsed,'held_ms':held,'settle_ms':settle,'key':key,'shots':shots,'weapon_counters':counts,'hit_callbacks':hits,'object_callback_events':objects,'landscape_callback_events':land,'last_fire_ms':last,'stop_ms':stop})
 assert offset==len(weapon_rows) and observations[0]==observations[1]
 assert record['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and record['stdout'].count('readback_present=PASS')==8 and record['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==2
 images={};digests=[]
 for index in range(8):
  p=BASE/f'runtime/output/map-frame-{index}.png';w,h,rgb=decode(p.read_bytes(),allow_alpha=True);assert (w,h)==(800,600);count=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert count>=64;digest=hashlib.sha256(rgb).hexdigest();digests.append(digest);images[str(p.relative_to(ROOT))]={'sha256':sha(p),'rgb_sha256':digest,'distinct_rgb':count}
 assert len(set(digests[:4]))==len(set(digests[4:]))==4
 snapshots={}
 for name in ('baseline-first-attempt','baseline-arcade-precondition'):
  folder=BASE/name;inventory=read(folder/'snapshot.json')
  for source,digest in inventory.items():assert sha(folder/Path(source).name)==digest
  snapshots[name]={'manifest_sha256':sha(folder/'snapshot.json'),'files':len(inventory)}
 debug=BASE/'debug/run-1.json';d=read(debug);assert sha(BASE/'debug/hosted-map-1.exe')==d['binary_sha256'] and sha(BASE/'debug/hosted-map-1.map')==d['mapping_sha256']
 assert b'Attempt to read from address 0000000000000030' in (BASE/'debug/cdb-1.log').read_bytes()
 sdk_base=ROOT/'build/matrixgame/e2e583-robot-orders/sdk';sdk=read(sdk_base/'result.json');assert sdk['status'].startswith('PASS') and not sdk['strong_undefined'];check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 metadata=read(sdk_base/'native-diagnostic-build.json');assert sha(sdk_base/'native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk_base/'source');check_hashes(metadata['rangers_host']['archives_sha256'])
 previous=read(ROOT/'reports/matrixgame-e2e583-robot-orders-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 nro=sdk_base/'diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(nro)=='53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233';assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 extra=[original,ROOT/'tests/matrixgame_firing_probe.hpp',ROOT/'tools/matrixgame_hosted_firing_checks.py',Path(__file__).resolve()]
 result={'status':'PASS; ORIGINAL ARCADE INPUT FIRE/STOP AND WEAPON CALLBACKS; TWO WINDOWS HAL HOSTED LOOPS; NO SWITCH','execution':record,'firing_observations':observations,'images':images,'original_weapon_body_restored_exactly':True,'extra_inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in extra},'preserved_fixture_failure_snapshots':snapshots,'debug_sha256':sha(debug),'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E583_NRO':{'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':sdk['build_id']},'new_NRO':False,'ASan_UBSan':'NOT RERUN; NO PRODUCTION CODE CHANGE','ARM64_QEMU':'NOT RERUN; NO PRODUCTION CODE CHANGE','Switch':'NOT RUN','remaining':['native/deko full Init/map/form/loop execution','Switch image/audio/HID/F9/focus/recovery','hostile damage, battle outcome and return to owning game'],'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(observations);print('E583 NRO preserved',sha(nro))
if __name__=='__main__':main()

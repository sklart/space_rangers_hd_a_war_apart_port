"""Independent evidence checks for actual original player-command movement."""
import argparse
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e584-hosted-movement'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 records=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);record=next(read(p) for p in records if read(p)['status'].startswith('PASS'))
 assert all(c['exit']==0 for c in record['commands']) and '/UNDEBUG' in record['flags'] and '/DNDEBUG' not in record['flags']
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(record[key])
 assert sha(BASE/'hosted-map.exe')==record['binary_sha256']
 command=re.findall(r'^MOVEMENT_COMMAND player_group=(\d+) start_cell=(\d+),(\d+) target_cell=(\d+),(\d+) world=([-\d.]+),([-\d.]+) map_time=(\d+) original_PlaceFindNear_PGOrderMoveTo=EXECUTED$',record['stdout'],re.M)
 results=re.findall(r'^MOVEMENT_RESULT elapsed=(\d+) start_cell=(\d+),(\d+) end_cell=(\d+),(\d+) target_cell=(\d+),(\d+) delta_world=([-\d.]+),([-\d.]+) remaining2=(\d+) initial2=(\d+) orders=(\d+)$',record['stdout'],re.M)
 assert len(command)==len(results)==2;observations=[]
 for issued,completed in zip(command,results):
  group,x0,y0,tx,ty=map(int,issued[:5]);wx,wy=map(float,issued[5:7]);time=int(issued[7]);elapsed,x,y,end_x,end_y,target_x,target_y=map(int,completed[:7]);dx,dy=map(float,completed[7:9]);remaining,initial,orders=map(int,completed[9:])
  assert (x,y,target_x,target_y)==(x0,y0,tx,ty) and elapsed==5000 and time==125
  assert remaining==(tx-end_x)**2+(ty-end_y)**2 and initial==(tx-x0)**2+(ty-y0)**2 and remaining<initial
  distance=math.hypot(dx,dy);assert math.isfinite(distance) and distance>=10 and orders==0 and (end_x,end_y)!=(x0,y0)
  observations.append({'group':group,'start_cell':[x,y],'end_cell':[end_x,end_y],'requested_cell':[tx,ty],'start_world':[wx,wy],'delta_world':[dx,dy],'distance_world':distance,'elapsed_ms':elapsed,'initial_distance_cells_squared':initial,'remaining_distance_cells_squared':remaining,'orders_remaining':orders})
 assert observations[0]==observations[1]
 assert record['stdout'].count('MOVEMENT original_player_command_navigation_simulation=PASS no_position_injection=PASS live_robot=PASS')==2
 assert record['stdout'].count('HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125 input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS')==2
 assert record['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and record['stdout'].count('readback_present=PASS')==8
 assert record['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==2
 images={};digests=[]
 for index in range(8):
  path=BASE/f'runtime/output/map-frame-{index}.png';w,h,rgb=decode(path.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  colors=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert colors>=64;digest=hashlib.sha256(rgb).hexdigest();digests.append(digest);images[str(path.relative_to(ROOT))]={'sha256':sha(path),'rgb_sha256':digest,'distinct_rgb':colors}
 assert len(set(digests[:4]))==4 and len(set(digests[4:]))==4
 journals={}
 for path in (BASE/'runtime/output/journals').rglob('*.jsonl'):
  rows=[json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line]
  if rows and rows[0]['source_sha256']==record['composition_source_sha256']:
   assert any(r['event']=='ok' and r['operation']=='B.CGame::Init' for r in rows) and not any(r['event']=='fail' for r in rows);journals[str(path.relative_to(ROOT))]=sha(path)
 assert len(journals)==2
 sdk_base=ROOT/'build/matrixgame/e2e583-robot-orders/sdk';sdk=read(sdk_base/'result.json');assert sdk['status'].startswith('PASS') and not sdk['strong_undefined'];check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 metadata=read(sdk_base/'native-diagnostic-build.json');assert sha(sdk_base/'native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk_base/'source');check_hashes(metadata['rangers_host']['archives_sha256'])
 previous=read(ROOT/'reports/matrixgame-e2e583-robot-orders-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 old=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=old['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==old['root_cumulative_NRO_verified_against_documented_hash']
 nro=sdk_base/'diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(nro)=='53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233';assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 extra=[ROOT/'tests/matrixgame_movement_probe.hpp',Path(__file__).resolve(),ROOT/'tools/matrixgame_hosted_movement_checks.py']
 result={'status':'PASS; GENUINE PLAYER MOVE COMMAND AND ORIGINAL NAVIGATION/SIMULATION; TWO WINDOWS HAL HOSTED LOOPS; NO SWITCH','execution':record,'movement_observations':observations,'images':images,'completed_journals_sha256':journals,'extra_inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in extra},'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E583_NRO':{'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':sdk['build_id']},'new_NRO':False,'ASan_UBSan':'NOT RERUN; NO PRODUCTION CODE CHANGE','ARM64_QEMU':'NOT RERUN; NO PRODUCTION CODE CHANGE','Switch':'NOT RUN','remaining':['native/deko full Init/map/form/loop execution','Switch image/audio/HID/F9/focus/recovery','full combat outcome and return to owning game'],'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(observations);print('E583 NRO preserved',sha(nro))
if __name__=='__main__':main()

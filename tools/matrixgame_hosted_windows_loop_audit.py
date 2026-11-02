"""Evidence for original interactive loop with real owning Rangers host."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e581-hosted-windows-loop'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 records=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True)
 record=next(read(path) for path in records if read(path)['status'].startswith('PASS'))
 assert all(c['exit']==0 for c in record['commands'])
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(record[key])
 assert sha(BASE/'hosted-map.exe')==record['binary_sha256']
 marker='HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125 input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS'
 assert record['stdout'].count(marker)==2
 assert record['stdout'].count('full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS')==2
 assert record['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and record['stdout'].count('readback_present=PASS')==8
 assert record['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==2
 assert '/DNDEBUG' not in record['flags'] and '/UNDEBUG' in record['flags']
 images={};rgb_hashes=[]
 for index in range(8):
  path=BASE/f'runtime/output/map-frame-{index}.png';w,h,rgb=decode(path.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  digest=hashlib.sha256(rgb).hexdigest();rgb_hashes.append(digest)
  colors=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert colors>=64
  images[str(path.relative_to(ROOT))]={'sha256':sha(path),'rgb_sha256':digest,'distinct_rgb':colors,'size':[w,h]}
 assert len(set(rgb_hashes[:4]))==4 and len(set(rgb_hashes[4:]))==4
 journals={}
 for path in (BASE/'runtime/output/journals').rglob('*.jsonl'):
  rows=[json.loads(line) for line in path.read_text().splitlines() if line]
  if rows and rows[0]['source_sha256']==record['composition_source_sha256']:
   assert any(r['event']=='ok' and r['operation']=='B.CGame::Init' for r in rows)
   assert not any(r['event']=='fail' for r in rows);journals[str(path.relative_to(ROOT))]=sha(path)
 assert len(journals)==2
 sdk_base=ROOT/'build/matrixgame/e2e580-loading-capture/sdk';sdk=read(sdk_base/'result.json');assert sdk['status'].startswith('PASS') and not sdk['strong_undefined']
 check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 metadata=read(sdk_base/'native-diagnostic-build.json');assert sha(sdk_base/'native-diagnostic-build.json')==sdk['metadata_sha256']
 check_hashes(metadata['generated_original_sha256'],sdk_base/'source');check_hashes(metadata['rangers_host']['archives_sha256'])
 # The sole changed historical build input is the shared test runner's new
 # optional probe/source list. All compiled production input hashes must match.
 delta={name:{'at_E580_build':value,'now':sha(ROOT/name)} for name,value in metadata['own_source_sha256'].items() if sha(ROOT/name)!=value}
 assert set(delta)=={'tools/matrixgame_hosted_windows_map_checks.py'},delta
 nro=sdk_base/'diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(nro)=='b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b'
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 game=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/host/result.json')['protected_game_sha256'];check_hashes(game)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; REAL WINDOWS HAL HOSTED ORIGINAL FORM LOOP; TWO INTERACTIVE SIMULATION/RECOVERY/CLEANUP CYCLES; NO SWITCH','execution':record,'images':images,'completed_journals_sha256':journals,'clean_upstream_pins':pins,'protected_game_sha256':game,'protected_frozen_sha256':protected,'preserved_E580_NRO':{'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':sdk['build_id']},'historical_E580_nonproduction_delta':delta,'new_NRO':False,'Switch':'NOT RUN','ASan_UBSan':'NOT RERUN; NO PRODUCTION CODE CHANGE','ARM64_QEMU':'NOT RERUN; NO PRODUCTION CODE CHANGE','remaining':['native/deko full Init/map/form/loop runtime on Switch','Switch input/audio/focus/screenshot/recovery','full battle outcome and return to owning game'],'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print('sessions=2 loop_draws=416 loop_simulated_ms=10250 distinct_frames=8 NRO_preserved='+sha(nro))
if __name__=='__main__':main()

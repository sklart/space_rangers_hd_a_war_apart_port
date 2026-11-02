"""Audit two order queue baselines, genuine-world oracle and SDK artifact."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e583-robot-orders'
def latest(folder,pattern='result*.json'):
 for path in sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True):
  value=read(path)
  if value['status'].startswith('PASS'):return value
 raise AssertionError('no PASS in '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 baselines={}
 for mode,error in (('remove','memcpy-param-overlap'),('rotate','use-after-poison')):
  folder=BASE/('baseline-'+mode);record=read(folder/'result.json');assert record['commands'][-1]['exit']==1 and record['commands'][-1]['label']=='runtime'
  for key in ('link_inputs_sha256','sources_sha256'):check_hashes(record[key])
  path=ROOT/record['commands'][-1]['log'];text=path.read_text(encoding='utf-8');assert 'ERROR: AddressSanitizer: '+error in text and 'ABORTING' in text
  if mode=='rotate':assert 'READ of size 63' in text and 'Poisoned by user' in text
  baselines[mode]={'record':record,'log_sha256':sha(path),'executable_sha256':sha(folder/'hosted-map.exe'),'expected_error':error}
 fixed=latest(BASE/'fixed');assert all(c['exit']==0 for c in fixed['commands']) and '/fsanitize=address' in fixed['flags'] and '/UNDEBUG' in fixed['flags'] and '/DNDEBUG' not in fixed['flags']
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(fixed[key])
 marker='ROBOT_ORDER genuine_idle_world_robot=PASS removals=15 rotations=60 all_fields=PASS logical_read_boundary=PASS object_core_refs=PASS original_empty_restored=PASS'
 assert fixed['stdout'].count(marker)==2 and 'ERROR: AddressSanitizer' not in fixed['stdout']
 assert fixed['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and fixed['stdout'].count('readback_present=PASS')==8
 native=latest(BASE/'native-build','build-result*.json');check_hashes(native['archives_sha256']);check_hashes(native['image_inputs_sha256']);check_hashes(native['generated']['files_sha256'],BASE/'native-build/native/source')
 before=read(ROOT/'build/matrixgame/e2e582-ai-ignore/native-build/build-result.json')['generated']['files_sha256'];after=native['generated']['files_sha256'];delta={name for name in set(before)|set(after) if before.get(name)!=after.get(name)};assert delta=={'MatrixGame/src/MatrixRobot.cpp'},delta
 source=(BASE/'native-build/native/source/MatrixGame/src/MatrixRobot.cpp').read_text(encoding='utf-8');assert 'sizeof(m_OrdersList[0]) * (m_OrdersInPool - 1)' in source and 'memcpy(m_OrdersList + pos' not in source
 profiles={}
 for kind in ('host','arm64'):
  record=latest(BASE/('compile-'+kind));assert record['runtime']=='NOT RUN' and all(c['exit']==0 for c in record['commands']);check_hashes(record['source_sha256']);check_hashes(record['objects_sha256']);profiles[kind]=record
 images={}
 for index in range(8):
  path=BASE/f'fixed/runtime/output/map-frame-{index}.png';w,h,rgb=decode(path.read_bytes(),allow_alpha=True);assert (w,h)==(800,600) and len({rgb[i:i+3] for i in range(0,len(rgb),3)})>=64;images[str(path.relative_to(ROOT))]=sha(path)
 sdk=latest(BASE/'sdk');assert all(c['exit']==0 for c in sdk['commands']) and not sdk['strong_undefined'];check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 assert 'CMatrixRobotAI::' in (BASE/'sdk/symbols.log').read_text()
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 previous=read(ROOT/'reports/matrixgame-e2e582-ai-ignore-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 previous_nro=ROOT/'build/matrixgame/e2e582-ai-ignore/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(previous_nro)=='2519b5122f556cf9be1cbd8b53126e1fd8a9e50d833c68c4d763862afa2e0d5e'
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 nro=BASE/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';result={'status':'PASS; TWO ORIGINAL ROBOT ORDER BUGS REPRODUCED AND FIXED; REAL WORLD PARTIAL MSVC ASAN; SDK NRO NOT RUN','baselines':baselines,'fixed':fixed,'native_build':native,'generated_delta':sorted(delta),'cross_compile_only':profiles,'images_sha256':images,'sdk':sdk,'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'E582_NRO_preserved':sha(previous_nro),'new_NRO':{'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':sdk['build_id']},'Switch':'NOT RUN','remaining':['native/deko full Init/map/loop runtime','hardware image/audio/HID/F9/focus/recovery','full battle outcome and return'],'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(result['new_NRO'])
if __name__=='__main__':main()

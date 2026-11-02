"""Audit actual overlap reproduction, original AI oracle and separate SDK NRO."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e582-ai-ignore'
def latest(folder,pattern='result*.json'):
 for path in sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True):
  result=read(path)
  if result['status'].startswith('PASS'):return result
 raise AssertionError('no completed PASS in '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 baseline=read(BASE/'baseline/result-1.json');assert baseline['commands'][-1]['label']=='runtime' and baseline['commands'][-1]['exit']==1
 log=ROOT/baseline['commands'][-1]['log'];text=log.read_text();assert 'ERROR: AddressSanitizer: memcpy-param-overlap' in text
 assert 'ABORTING' in text;check_hashes(baseline['link_inputs_sha256'])
 original=BASE/'baseline-environment.cpp';assert sha(original)==baseline['sources_sha256'][str(original)]
 fixed=latest(BASE/'fixed');assert all(c['exit']==0 for c in fixed['commands'])
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(fixed[key])
 assert '/fsanitize=address' in fixed['flags'] and '/UNDEBUG' in fixed['flags'] and '/DNDEBUG' not in fixed['flags']
 assert fixed['stdout'].count('AI_IGNORE real_map_identities=17 overflow_shifts=65 FIFO_pointer_time=PASS fresh_timestamp=PASS duplicate_refresh=PASS deleted_slot=PASS expiry_boundary=PASS')==2
 assert fixed['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and fixed['stdout'].count('readback_present=PASS')==8
 assert 'ERROR: AddressSanitizer' not in fixed['stdout']
 native=latest(BASE/'native-build','build-result*.json');check_hashes(native['archives_sha256']);check_hashes(native['image_inputs_sha256'])
 before=read(ROOT/'build/matrixgame/e2e580-loading-capture/native-build/build-result.json')['generated']['files_sha256'];after=native['generated']['files_sha256']
 changed={name for name in set(before)|set(after) if before.get(name)!=after.get(name)};assert changed=={'MatrixGame/src/Logic/MatrixEnvironment.cpp'},changed
 check_hashes(after,BASE/'native-build/native/source')
 current=(BASE/'native-build/native/source/MatrixGame/src/Logic/MatrixEnvironment.cpp').read_text()
 assert 'std::memmove(m_Ignore,' in current and 'std::memmove(m_IgnoreTime,' in current and 'm_IgnoreTime[m_IgnoreCnt - 1] = g_MatrixMap->GetTime();' in current
 cross={}
 for kind in ('host','arm64'):
  record=latest(BASE/('compile-'+kind));assert all(c['exit']==0 for c in record['commands']) and record['runtime']=='NOT RUN'
  check_hashes(record['source_sha256']);check_hashes(record['objects_sha256']);cross[kind]=record
 images={}
 for index in range(8):
  path=BASE/f'fixed/runtime/output/map-frame-{index}.png';w,h,rgb=decode(path.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  assert len({rgb[i:i+3] for i in range(0,len(rgb),3)})>=64;images[str(path.relative_to(ROOT))]=sha(path)
 sdk=latest(BASE/'sdk');assert not sdk['strong_undefined'] and all(c['exit']==0 for c in sdk['commands'])
 check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 assert 'CInfo::AddIgnore(' in (BASE/'sdk/symbols.log').read_text()
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 old=read(ROOT/'reports/matrixgame-e2e580-loading-capture-evidence.json');check_hashes(old['protected_game_sha256']);check_hashes(old['protected_frozen_sha256'])
 previous=ROOT/'build/matrixgame/e2e580-loading-capture/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(previous)=='b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b'
 previous_evidence=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=previous_evidence['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous_evidence['root_cumulative_NRO_verified_against_documented_hash']
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 nro=BASE/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
 result={'status':'PASS; ORIGINAL AI IGNORE OVERLAP REPRODUCED AND FIXED; REAL MAP MSVC PARTIAL ASAN; SDK NRO NOT RUN','baseline_log_sha256':sha(log),'baseline_executable_sha256':sha(BASE/'baseline/hosted-map.exe'),'baseline':baseline,'fixed':fixed,'native_build':native,'generated_delta':sorted(changed),'cross_compile_only':cross,'images_sha256':images,'sdk':sdk,'clean_upstream_pins':pins,'protected_game_sha256':old['protected_game_sha256'],'protected_frozen_sha256':old['protected_frozen_sha256'],'E580_NRO_preserved':sha(previous),'new_NRO':{'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':sdk['build_id']},'Switch':'NOT RUN','remaining':['full native/deko Init/map/loop runtime','Switch image/audio/HID/F9/focus/recovery','full battle outcome and return'],'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(result['new_NRO'])
if __name__=='__main__':main()

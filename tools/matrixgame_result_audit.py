"""Audit original surrender/statistics and the native SaveResult integration."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_screenshot_png_verify import decode
BASE=ROOT/'build/matrixgame/e2e589-original-result'

def latest(folder,pattern):
 paths=sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True)
 result=read(paths[0]);assert result['status'].startswith('PASS');return result

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 baseline=read(BASE/'baseline/result.json');assert baseline['status'].startswith('EXPECTED FAILURE')
 assert baseline['commands'][0]['exit']==0 and baseline['commands'][1]['exit']==23
 check_hashes(baseline['inputs_sha256']);assert sha(BASE/'baseline/probe.exe')==baseline['binary_sha256']
 assert sha(BASE/'baseline/native_diagnostic_session.hpp')==baseline['E586_header_sha256']
 snapshot=read(BASE/'sdk-first-attempt/snapshot.json')
 for name,digest in snapshot.items():assert sha(BASE/'sdk-first-attempt'/Path(name).name)==digest
 profiles={}
 for kind in ('windows','host','arm64'):
  r=latest(BASE/'policy'/kind,'result*.json');assert all(c['exit']==0 for c in r['commands']);check_hashes(r['source_sha256'])
  assert 'surrender4=PASS exception100_104_rejected=PASS' in r['stdout'];assert sha(BASE/'policy'/kind/('session.exe' if kind=='windows' else 'session'))==r['binary_sha256'];profiles[kind]=r
 runtime=latest(BASE/'windows','result*.json');assert all(c['exit']==0 for c in runtime['commands'])
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(runtime[key])
 assert '/UNDEBUG' in runtime['flags'] and '/DNDEBUG' not in runtime['flags'];assert sha(BASE/'windows/hosted-map.exe')==runtime['binary_sha256']
 assert str(ROOT/'port/matrixgame/native-engine/original_game.cpp') in runtime['sources_sha256']
 for marker in ('RESULT_INPUT menu_ESC=PASS','RESULT_INPUT surrender_S_confirmation=PASS','RESULT_INPUT confirmed_surrender_original_state4=PASS','RESULT_INPUT statistics_Enter_original_exit=PASS','six_original_stats=PASS pre_post_reject_unchanged=PASS owning_host_live=PASS','HOSTED_MAP final_host_cleanup=PASS','actual gamma/window readback restored; device/timer leases empty'):
  assert runtime['stdout'].count(marker)==2,marker
 rows=[dict(p.split('=',1) for p in line.split()[1:]) for line in runtime['stdout'].splitlines() if line.startswith('ORIGINAL_RESULT ')]
 assert len(rows)==2 and all(int(r['state'])==4 for r in rows)
 # Original STAT_TIME is a statistic, not our deterministic map-loop clock.
 # Compare its genuine per-run value with the source in the C++ oracle above;
 # do not demand identical wall-clock-derived durations between sessions.
 assert all({k:v for k,v in r.items() if k!='time'}=={k:v for k,v in rows[0].items() if k!='time'} for r in rows)
 assert runtime['stdout'].count('HOSTED_ORIGINAL_LOOP draws=208 elapsed=5050')==2 and runtime['stdout'].count('readback_present=PASS')==8
 images={};digests=[]
 for index in range(8):
  p=BASE/f'windows/runtime/output/map-frame-{index}.png';w,h,rgb=decode(p.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  count=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert count>=64;digest=hashlib.sha256(rgb).hexdigest();digests.append(digest)
  images[str(p.relative_to(ROOT))]={'sha256':sha(p),'rgb_sha256':digest,'distinct_rgb':count}
 assert len(set(digests[:4]))==len(set(digests[4:]))==4
 variants=latest(BASE/'sdk','variants-result*.json');manual=variants['interactive'];assert not variants['quick_strong_undefined'] and not manual['strong_undefined']
 assert all(c['exit']==0 for c in [*variants['commands'],*manual['commands']]);check_hashes(manual['link_inputs_sha256']);check_hashes(variants['all_link_inputs_sha256']);check_hashes(variants['embedded_fonts_sha256']);check_hashes(manual['protected_sha256'])
 for name,value in {**manual['artifacts'],**variants['quick_artifacts']}.items():assert sha(ROOT/name)==value['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==manual['metadata_sha256']
 check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 old_metadata=read(ROOT/'build/matrixgame/e2e586-interactive-diagnostic/sdk/native-diagnostic-build.json');assert metadata['generated_original_sha256']==old_metadata['generated_original_sha256']
 packages={}
 for target,folder,title in (('matrixgame-native-interactive','interactive-nro','MatrixGame diagnostic manual'),('matrixgame-native-diagnostic','diagnostic-nro','MatrixGame diagnostic')):
  nro=BASE/'sdk'/folder/(target+'.nro');nacp=BASE/'sdk'/folder/'control.nacp';elf=BASE/'sdk'/(target+'.elf')
  validation=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes())
  assert nacp.read_bytes()[:0x200].split(b'\0')[0].decode('utf-8')==title
  payload=elf.read_bytes()
  for marker in (b'C.original_SaveResult',b'original_time_signed_ms',b'original_buildings_destroyed'):assert marker in payload
  packages[target]={'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':manual['build_id'],'validation':validation,'Switch':'NOT RUN'}
 symbols=(BASE/'sdk/symbols.log').read_text(encoding='utf-8');quick_symbols=(BASE/'sdk/quick-symbols.log').read_text(encoding='utf-8')
 for text in (symbols,quick_symbols):assert 'CGame::SaveResult(' in text and 'NativeInitRuntime::SaveOriginalGameResult(' in text
 old=read(ROOT/'reports/matrixgame-e2e586-interactive-diagnostic-final-evidence.json');check_hashes(old['protected_game_sha256']);check_hashes(old['protected_frozen_sha256'])
 for package in old['packages'].values():assert sha(ROOT/package['path'])==package['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; ORIGINAL SURRENDER4 AND SIX SAVE_RESULT STATS; TWO WINDOWS HAL SESSIONS; POLICY ASAN/ARM64; TWO SDK NROS NOT RUN',
  'policy_profiles':profiles,'windows_execution':runtime,'result_snapshots':rows,'images':images,'variants_build':variants,'packages':packages,
  'exact_E586_surrender4_failure':baseline,'preserved_first_SDK_attempt_sha256':snapshot,
  'original_generated_sources_unchanged':True,'preserved_E586_packages':old['packages'],'protected_game_sha256':old['protected_game_sha256'],'protected_frozen_sha256':old['protected_frozen_sha256'],'clean_upstream_pins':pins,
  'SaveResult_sanitizer_ARM64_runtime':'NOT RUN; ORIGINAL LIVE SAVE_RESULT EXECUTED WINDOWS HAL ONLY','Switch':'NOT RUN','full_battle_win_loss':'NOT PROVEN','owning_galaxy_transition':'NOT RUN; RESULT SNAPSHOT OUTLIVES MATRIXGAME CLEANUP WITH HOST LIVE',
  'remaining':['full native/deko Init and visible map frames','Switch image/audio/HID/F9/focus/recovery','full battle victory/loss and owning galaxy transition'],
  'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(rows);print(packages)
if __name__=='__main__':main()

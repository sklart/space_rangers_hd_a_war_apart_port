"""Audit a real Run-slot implementation without claiming a Galaxy transition."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run'
def latest(folder):
 p=max(folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns);r=read(p);assert r['status'].startswith('PASS');return r
def fields(line):return dict(p.split('=',1) for p in line.split()[1:])
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 executions={};observations={}
 for profile,count in (('windows-asan',2),('windows-asan-settings',1)):
  r=latest(BASE/profile);assert all(c['exit']==0 for c in r['commands']);check_hashes(r['sources_sha256']);check_hashes(r['link_inputs_sha256']);assert sha(BASE/profile/'rangers-run.exe')==r['binary_sha256']
  assert '/fsanitize=address' in r['flags'] and '/UNDEBUG' in r['flags'] and '/DNDEBUG' not in r['flags']
  assert r['stdout'].count('null_output_and_Init_failure=REJECTED output_unchanged=PASS cleanup_retry_ready=PASS copied_context=PASS')==count
  assert r['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==count and r['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==count
  rows=[fields(l) for l in r['stdout'].splitlines() if l.startswith('RANGERS_RUN_RESULT ')];assert len(rows)==count
  for row in rows:
   assert int(row['code'])==2 and int(row['signed_time'])>0 and int(row['robots_destroyed'])>0 and int(row['turrets_destroyed'])>0
   assert int(row['warming'])==30 and int(row['draws'])>30 and 0<int(row['ticks'])<=120000
   assert all(row[k]=='PASS' for k in ('typed_Run','original_stats','owner_cleanup_before_return','host_alive','legacy_tokens_not_pointer_casts'))
  assert not any(s in r['stdout'] for s in ('ERROR: AddressSanitizer','AddressSanitizer: heap','AddressSanitizer: stack'))
  if profile.endswith('settings'):assert 'caller_settings_nonnull=PASS caller_bytes_unchanged=PASS' in r['stdout']
  observations[profile]=rows;executions[profile]=r
 cross={}
 for kind in ('host','arm64'):
  r=latest(BASE/('compile-'+kind));assert r['runtime']=='NOT RUN' and all(c['exit']==0 for c in r['commands']);check_hashes(r['source_sha256']);check_hashes(r['objects_sha256']);cross[kind]=r
 sdk=latest(BASE/'sdk');assert sdk['runtime']=='NOT RUN' and not sdk['strong_undefined'] and len(sdk['retained_API_symbols'])>=6 and all(c['exit']==0 for c in sdk['commands'])
 check_hashes(sdk['link_inputs_sha256'])
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 old=read(ROOT/'build/matrixgame/e2e590-completed-battle/sdk/native-diagnostic-build.json');assert metadata['generated_original_sha256']==old['generated_original_sha256']
 prior=read(ROOT/'reports/matrixgame-e2e590-completed-battle-evidence.json');check_hashes(prior['protected_game_sha256']);check_hashes(prior['protected_frozen_sha256'])
 for package in prior['packages'].values():assert sha(ROOT/package['path'])==package['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash'];assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 extra=[ROOT/'tools/matrixgame_native_rangers_run_settings_windows.py',Path(__file__).resolve()]
 result={'status':'PARTIAL; REAL TYPED RUN SLOT IMPLEMENTED; THREE WINDOWS HAL MISSION LOSSES UNDER PARTIAL ASAN; SDK API LINK PASS; FULL GALAXY CALLER NOT RUN',
  'windows_executions':executions,'mission_results':observations,'cross_compile_only':cross,'SDK':sdk,'preserved_E590_packages':prior['packages'],'generated_originals_unchanged':True,
  'extra_inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in extra},'clean_upstream_pins':pins,'protected_game_sha256':prior['protected_game_sha256'],'protected_frozen_sha256':prior['protected_frozen_sha256'],
  'new_NRO':False,'SDK_link_ELF_runtime':'NOT RUN','full_Galaxy_FRun_fGov_transition':'NOT RUN','full_victory':'NOT RUN','Switch':'NOT RUN',
  'remaining':['real Galaxy/Player/government GI-tree and renderer session','native Initialize/Finalize/Support integration for the complete Robot interface','actual FRun caller and reward/history/galaxy transition','native/deko hardware execution'],'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(observations);print(sdk['artifacts'])
if __name__=='__main__':main()

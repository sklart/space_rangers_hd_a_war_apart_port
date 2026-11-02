"""Audit the original script host lifecycle without claiming Galaxy or battle completion."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e595-script-session'
def latest(folder):
 path=max(folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns)
 r=read(path);assert r['status'].startswith('PASS'),path
 assert all(c['exit']==0 for c in r['commands'])
 return r
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 windows=latest(BASE/'windows-asan');cpu={k:latest(BASE/k) for k in ('asan','arm64')}
 for r in (windows,*cpu.values()):
  assert r['stdout'].count('original_VM_abs_assignment=42 path_allocate_recycle=PASS auto_manual_events=PASS real_worker_standby=PASS Galaxy=NOT_CREATED')==2
  assert r['stdout'].count('both_workers_joined=PASS all_14_events_closed=PASS builtin_containers_released=PASS foreign_stack=REJECTED')==2
  assert 'original_Finalize=PASS events_worker_closed=PASS pool_heads_counts_callback_restored=PASS foreign_owner=REJECTED no_save_load=PASS NO_WORLD_GENERATION' in r['stdout']
  assert 'ERROR: AddressSanitizer' not in r['stdout'] and 'runtime error:' not in r['stdout']
  for key in ('sources_sha256','source_sha256','link_inputs_sha256','reused_objects_sha256','objects_sha256','libraries_sha256'):
   check_hashes(r.get(key,{}))
 assert sha(BASE/'windows-asan/script-session.exe')==windows['binary_sha256']
 assert '/fsanitize=address' in windows['flags'] and '/UNDEBUG' in windows['flags'] and '/DNDEBUG' not in windows['flags']
 for kind,r in cpu.items():assert sha(BASE/kind/'government')==r['binary_sha256']
 sdk=latest(BASE/'sdk');assert sdk['runtime']=='NOT RUN' and not sdk['strong_undefined'] and len(sdk['retained_API_symbols'])==4
 check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in {**sdk['artifacts'],**sdk['preserved_E592_artifacts']}.items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256']
 check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source')
 previous=read(ROOT/'reports/matrixgame-e2e594-government-session-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(previous[key])
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint']
 assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
  assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 archive=Path('D:/logs2.7z');external={'path':str(archive),'status':'NOT VERIFIED; EXTERNAL FILE ABSENT'}
 if archive.is_file():
  external.update(status='PASS; CURRENT SHA256 MATCHES',sha256=sha(archive));assert external['sha256']=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 evidence={'status':'PARTIAL; SCRIPT VM/TWO WORKERS/14 EVENTS/PATH/BUILTIN CONTAINERS PASS; GALAXY NOT CREATED',
  'Windows_partial_ASan':windows,'CPU_runtime':cpu,'SDK':sdk,'external_logs2_archive':external,'clean_upstream_pins':pins,
  'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],
  'preserved_E590_packages':previous['preserved_E590_packages'],'primary_NRO':primary,
  'debug_evidence_sha256':{str(p.relative_to(ROOT)):sha(p) for p in (BASE/'asan/debug').glob('*') if p.is_file()},
  'audit_sha256':sha(Path(__file__).resolve()),'new_NRO':False,'commit':False,'push':False,
  'remaining':['original Galaxy/Player/Planet generation with real Hangar and TCCInterface owners','original Gov OnOpen/rewards/history/return transition','natural victory','native/deko FRun runtime and user hardware acceptance']}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status']);print(sdk['artifacts'])
if __name__=='__main__':main()

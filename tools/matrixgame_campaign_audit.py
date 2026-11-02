"""Audit the generated original campaign without claiming battle/Gov return proof."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
BASE=ROOT/'build/matrixgame/e2e598-campaign'
def latest(folder,attempts=False):
 path=max(folder.glob('*/result.json') if attempts else folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns);r=read(path)
 assert r['status'].startswith('PASS'),path;assert all(c['exit']==0 for c in r['commands']);return r
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);output=p.parse_args().output
 runtime={k:latest(BASE/k,True) for k in ('windows-asan','asan','arm64')}
 for kind,r in runtime.items():
  assert 'original_generation_complete=PASS stage=8 stars=73' in r['stdout'] and 'real_Player_Star_HomePlanet=PASS duplicate_owner=REJECTED no_save_load=PASS' in r['stdout']
  assert 'original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN' in r['stdout']
  assert 'ERROR: AddressSanitizer' not in r['stdout'] and 'runtime error:' not in r['stdout']
  for key in ('sources_sha256','link_inputs_sha256','reused_objects_sha256','objects_sha256','libraries_sha256'):check_hashes(r.get(key,{}))
  if kind=='windows-asan':
   attempt=BASE/kind/str(r['attempt']);assert sha(attempt/'campaign.exe')==r['binary_sha256'];assert '/fsanitize=address' in r['flags'] and '/UNDEBUG' in r['flags']
  else:assert sha(ROOT/r['binary'])==r['binary_sha256']
 dep=latest(BASE/'sdk-dependency');check_hashes(dep['source_sha256']);check_hashes(dep['dependency_objects_sha256']);assert dep['changed_members']==['fGameSettings.o'] and sha(ROOT/dep['archive_path'])==dep['dependency_archive_sha256'];assert members(ROOT/dep['archive_path'])==dep['archive_members_sha256']
 sdk=latest(BASE/'sdk');assert not sdk['strong_undefined'] and sdk['runtime']=='NOT RUN';check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in {**sdk['artifacts'],**sdk['preserved_E592_artifacts']}.items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256']);assert metadata['rangers_host']['dependency_manifest_sha256']==sha(BASE/'sdk-dependency/result.json')
 previous=read(ROOT/'reports/matrixgame-e2e597-global-ui-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(previous[key])
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 primary=previous['primary_NRO'];assert sha(Path(primary['path']))==primary['sha256'];font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 pins={};git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PARTIAL; ORIGINAL CAMPAIGN/PLAYER/STARS/PLANETS/WARMUP/CLEANUP PASS; BATTLE/GOV RETURN NOT RUN','runtime':runtime,'SDK_dependency':dep,'SDK':sdk,'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E590_packages':previous['preserved_E590_packages'],'primary_NRO':primary,'new_NRO':False,'commit':False,'push':False,'remaining':['original Gov OnOpen battle result/rewards/history/return','natural victory','native/deko FRun runtime and user hardware acceptance'],'audit_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as f:json.dump(evidence,f,indent=2);f.write('\n')
 print(evidence['status']);print(sdk['artifacts'])
if __name__=='__main__':main()

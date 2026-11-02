"""Audit the scoped cleanup gates and protected inputs; no battle/hardware claims."""
import argparse, json, subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
from matrixgame_campaign_cleanup_checks import BASE

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 paths={'host_ASan_UBSan':BASE/'asan/5/result.json','array_lifetime':BASE/'array-lifetime/1/result.json','Windows_partial_ASan':BASE/'windows/1/result.json','ARM64_QEMU':BASE/'arm64/2/result.json','SDK_dependency':BASE/'sdk-dependency/result.json','SDK':BASE/'sdk/result.json'}
 gates={name:read(path) for name,path in paths.items()}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS'),(name,gate['status']);assert all(c['exit']==0 for c in gate['commands']),name
 for name in ('host_ASan_UBSan','Windows_partial_ASan','ARM64_QEMU'):
  gate=gates[name]
  for key in ('generated_source_sha256','sources_sha256','objects_sha256','reused_objects_sha256'):
   if key in gate:check_hashes(gate[key])
  path=paths[name].parent/('campaign.exe' if name=='Windows_partial_ASan' else 'campaign');assert sha(path)==gate['binary_sha256']
  assert 'original_generation_complete=PASS stage=8 stars=73' in gate['stdout'] and 'original_world_worker_cleanup=PASS' in gate['stdout']
  assert 'victory_Gov_return=NOT_RUN' in gate['stdout']
 array=gates['array_lifetime'];assert sha(ROOT/'tests/probe_matrixgame_script_array_lifetime.cpp')==array['source_sha256'];assert sha(ROOT/'port/switch/platform/script_array_lifetime.hpp')==array['lifetime_header_sha256'];assert sha(paths['array_lifetime'].parent/'array-lifetime')==array['binary_sha256']
 dep=gates['SDK_dependency'];check_hashes(dep['source_sha256']);check_hashes(dep['dependency_objects_sha256']);assert sha(ROOT/dep['archive_path'])==dep['dependency_archive_sha256'];assert members(ROOT/dep['archive_path'])==dep['archive_members_sha256'];assert len(dep['changed_members'])==7
 sdk=gates['SDK'];assert not sdk['strong_undefined'] and sdk['runtime']=='NOT RUN' and not sdk['new_NRO']
 check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in {**sdk['artifacts'],**sdk['preserved_E592_artifacts']}.items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 assert metadata['rangers_host']['dependency_manifest_sha256']==sha(paths['SDK_dependency'])
 prior=read(ROOT/'reports/matrixgame-e2e598-campaign-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(prior[key])
 for info in prior['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 assert sha(Path(prior['primary_NRO']['path']))==prior['primary_NRO']['sha256']
 root_nro=sha(ROOT/'Switch-E2E5-TEST.nro');assert root_nro==read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')['root_cumulative_NRO_verified_against_documented_hash']
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in prior['clean_upstream_pins'].items():
  path=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',path,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',path,'status','--porcelain'],text=True)
 archive=Path('D:/logs2.7z');external={'path':str(archive),'status':'NOT VERIFIED; EXTERNAL FILE ABSENT'}
 if archive.is_file():external.update(status='PRESENT; FINGERPRINT ONLY',sha256=sha(archive))
 evidence={'status':'PASS SCOPED CAMPAIGN CLEANUP GATES; FULL BATTLE/GOV RETURN NOT RUN','gates':gates,'gate_results_sha256':{str(p.relative_to(ROOT)):sha(p) for p in paths.values()},'clean_upstream_pins':pins,'protected_game_sha256':prior['protected_game_sha256'],'protected_frozen_sha256':prior['protected_frozen_sha256'],'preserved_E590_packages':prior['preserved_E590_packages'],'primary_NRO':prior['primary_NRO'],'root_NRO_sha256':root_nro,'external_logs2_archive':external,'new_NRO':False,'commit':False,'push':False,'Switch':'NOT RUN','remaining':['real campaign government entry and battle result/rewards/history/return','natural victory','native/deko FRun runtime','new integrated diagnostic NRO and manual hardware acceptance'],'audit_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status']);print(json.dumps(sdk['artifacts'],indent=2))

if __name__=='__main__':main()

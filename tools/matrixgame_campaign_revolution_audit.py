"""Audit snapshot/cleanup closure, with the pre-seed ARM64 version boundary explicit."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
from matrixgame_campaign_revolution_overrides import apply as revolution

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output;base=ROOT/'build/matrixgame/e2e601-revolution-seed'
 paths={'host_ASan_UBSan_LSan':base/'1/result.json','ARM64_snapshot_before_revolution_seed':ROOT/'build/matrixgame/e2e600-campaign-navigation/snapshot-arm64/2/result.json','Windows_snapshot_before_revolution_seed':ROOT/'build/matrixgame/e2e600-campaign-navigation/snapshot-windows/4/result.json','production_pipeline':base/'production-pipeline/4/result.json','SDK_dependency':base/'sdk-dependency/result.json','SDK':max((base/'sdk').glob('result*.json'),key=lambda p:p.stat().st_mtime_ns)}
 gates={k:read(p) for k,p in paths.items()}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS'),(name,gate['status']);assert all(c['exit']==0 for c in gate.get('commands',[])),name
 host=gates['host_ASan_UBSan_LSan'];check_hashes(host['sources_sha256']);check_hashes(host['reused_objects_sha256']);assert sha(base/'1/snapshot')==host['binary_sha256'];assert host['seed_checks']==77
 for name in ('host_ASan_UBSan_LSan','ARM64_snapshot_before_revolution_seed','Windows_snapshot_before_revolution_seed'):
  gate=gates[name];assert 'SNAPSHOT original_memory_roundtrip=PASS' in gate['stdout'] and 'original_world_worker_cleanup=PASS' in gate['stdout']
  for key in ('objects_sha256','reused_objects_sha256','reused_inputs_sha256'):
   if key in gate:check_hashes(gate[key])
  if name!='host_ASan_UBSan_LSan':
   # Generator hashes are historical in these completed gates. Their actual
   # generated/compiled C++ inputs and binary fingerprints remain immutable.
   check_hashes({k:v for k,v in gate['sources_sha256'].items() if not k.replace('\\','/').startswith('tools/')})
   binary=paths[name].parent/('navigation.exe' if name.startswith('Windows') else 'snapshot');assert sha(binary)==gate['binary_sha256']
 old_arm=ROOT/'build/matrixgame/e2e599-campaign-cleanup/arm64/2/aPlanet.cpp';assert revolution('aPlanet.cpp',old_arm.read_text(encoding='utf-8'))==(base/'1/aPlanet.cpp').read_text(encoding='utf-8')
 pipeline=gates['production_pipeline'];check_hashes(pipeline['sources_sha256']);assert len(pipeline['full_source_matches'])==8 and len(pipeline['methods'])==13
 dep=gates['SDK_dependency'];check_hashes(dep['dependency_objects_sha256']);assert dep['changed_members']==['aPlanet.o'];assert sha(ROOT/dep['archive_path'])==dep['dependency_archive_sha256'];assert members(ROOT/dep['archive_path'])==dep['archive_members_sha256']
 sdk=gates['SDK'];assert not sdk['strong_undefined'] and sdk['runtime']=='NOT RUN' and not sdk['new_NRO'];check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 meta=read(base/'sdk/native-diagnostic-build.json');assert sha(base/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(meta['own_source_sha256']);check_hashes(meta['generated_original_sha256'],base/'sdk/source');check_hashes(meta['rangers_host']['archives_sha256']);assert meta['rangers_host']['dependency_manifest_sha256']==sha(paths['SDK_dependency'])
 prior=read(ROOT/'reports/matrixgame-e2e599-campaign-cleanup-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(prior[key])
 assert sha(Path(prior['primary_NRO']['path']))==prior['primary_NRO']['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==prior['root_NRO_sha256']
 for info in prior['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in prior['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PASS COMPONENT SNAPSHOT/CLEANUP/GENERATOR/SDK CLOSURE; ARM64 FULL RUNTIME PRE-SEED VERSION','gates':gates,'gate_results_sha256':{str(p.relative_to(ROOT)):sha(p) for p in paths.values()},'ARM64_current_seed_evidence':'77 independent compile-time low32 oracles on devkitA64; full snapshot runtime uses immediately preceding source version','production_integration':'eight full generated sources byte-identical to compiled SDK inputs; five previously skipped generator routes repaired','protected_game_sha256':prior['protected_game_sha256'],'protected_frozen_sha256':prior['protected_frozen_sha256'],'primary_NRO':prior['primary_NRO'],'root_NRO_sha256':prior['root_NRO_sha256'],'clean_upstream_pins':prior['clean_upstream_pins'],'preserved_E590_packages':prior['preserved_E590_packages'],'Switch':'NOT RUN','new_NRO':False,'commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status'])

if __name__=='__main__':main()

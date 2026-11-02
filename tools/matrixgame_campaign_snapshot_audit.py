"""Strict evidence for original in-memory campaign restore; battle return remains open."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output;base=ROOT/'build/matrixgame/e2e600-campaign-navigation'
 paths={'Windows_partial_ASan':base/'snapshot-windows/4/result.json','host_ASan_UBSan_LSan':base/'snapshot-asan/2/result.json','ARM64_QEMU':base/'snapshot-arm64/2/result.json','SDK_dependency':base/'sdk-dependency/result.json','SDK':max((base/'sdk').glob('result*.json'),key=lambda p:p.stat().st_mtime_ns)}
 gates={name:read(path) for name,path in paths.items()}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS'),(name,gate['status']);assert all(c['exit']==0 for c in gate['commands']),name
 for name in ('Windows_partial_ASan','host_ASan_UBSan_LSan','ARM64_QEMU'):
  gate=gates[name]
  for key in ('sources_sha256','objects_sha256','reused_objects_sha256','reused_inputs_sha256','original_source_sha256'):
   if key in gate:check_hashes(gate[key])
  binary=paths[name].parent/('navigation.exe' if name=='Windows_partial_ASan' else 'snapshot');assert sha(binary)==gate['binary_sha256']
  for marker in ('NAVIGATION original_landing=PASS','GOVERNMENT original_OnOpen=PASS','SNAPSHOT original_memory_roundtrip=PASS','persistent_save_load_counters_unchanged=PASS no_save_files=PASS','CAMPAIGN original_world_worker_cleanup=PASS'):assert marker in gate['stdout'],(name,marker)
  assert 'victory_Gov_return=NOT_RUN' in gate['stdout']
  files=list((paths[name].parent/'runtime').rglob('*'));assert all(not p.is_file() or p.suffix.lower() in ('.log','.csv') for p in files),'unexpected private runtime output'
 dep=gates['SDK_dependency'];check_hashes(dep['source_sha256']);check_hashes(dep['dependency_objects_sha256']);assert dep['changed_members']==['aGalaxy.o','aShip.o'];assert sha(ROOT/dep['archive_path'])==dep['dependency_archive_sha256'];assert members(ROOT/dep['archive_path'])==dep['archive_members_sha256']
 sdk=gates['SDK'];assert not sdk['strong_undefined'] and sdk['runtime']=='NOT RUN' and not sdk['new_NRO'];check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 meta=read(base/'sdk/native-diagnostic-build.json');assert sha(base/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(meta['own_source_sha256']);check_hashes(meta['generated_original_sha256'],base/'sdk/source');check_hashes(meta['rangers_host']['archives_sha256']);assert meta['rangers_host']['dependency_manifest_sha256']==sha(paths['SDK_dependency'])
 previous=read(ROOT/'reports/matrixgame-e2e599-campaign-cleanup-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(previous[key])
 assert sha(Path(previous['primary_NRO']['path']))==previous['primary_NRO']['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_NRO_sha256']
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PASS SCOPED ORIGINAL CAMPAIGN/NAVIGATION/GOV/MEMORY SNAPSHOT; NO BATTLE RETURN','gates':gates,'gate_results_sha256':{str(p.relative_to(ROOT)):sha(p) for p in paths.values()},'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'primary_NRO':previous['primary_NRO'],'root_NRO_sha256':previous['root_NRO_sha256'],'clean_upstream_pins':previous['clean_upstream_pins'],'preserved_E590_packages':previous['preserved_E590_packages'],'Switch':'NOT RUN','new_NRO':False,'commit':False,'push':False,'remaining':['integrated actual Gov/Robot::FRun battle outcomes and rewards/history/return','natural victory','native/deko FRun runtime','separate updated diagnostic NRO','user hardware acceptance'],'audit_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status'])

if __name__=='__main__':main()

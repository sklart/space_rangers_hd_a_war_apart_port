"""Audit input queue, actual GI dispatch, SDK closure and preserved inputs."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();base=ROOT/'build/matrixgame/e2e609-rangers-input';paths={name:base/name/'1/result.json' for name in ('cpu','windows','sdk')};gates={k:read(p) for k,p in paths.items()};records={}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS') and all(c['exit']==0 for c in gate['commands']);check_hashes(gate['sources_sha256'])
  snapshot=read(paths[name].parent/'source-snapshot.json');assert snapshot['result_sha256']==sha(paths[name]) and set(snapshot['files'])==set(gate['sources_sha256'])
  for source,info in snapshot['files'].items():assert info['sha256']==gate['sources_sha256'][source] and sha(ROOT/info['snapshot'])==info['sha256']
  for key in ('reused_inputs_sha256','objects_sha256'):
   if key in gate:check_hashes(gate[key])
  for artifact,info in gate.get('artifacts',{}).items():assert sha(ROOT/artifact)==info['sha256']
  records[name]={'path':str(paths[name].relative_to(ROOT)),'sha256':sha(paths[name]),'source_snapshot_sha256':sha(paths[name].parent/'source-snapshot.json'),'status':gate['status']}
 cpu=paths['cpu'].parent
 for name in ('host','arm64'):
  log=(cpu/(name+'-runtime.log')).read_text(encoding='utf-8');assert 'exhaustive_letterbox_points=PASS' in log and 'bars_no_click=PASS' in log and 'drag_release=PASS' in log and 'bounded_queue=PASS' in log
 windows=gates['windows'];assert sha(paths['windows'].parent/'ui-context.exe')==windows['binary_sha256'];assert (paths['windows'].parent/'runtime.log').read_text(encoding='utf-8')==windows['stdout'];assert 'original_UI_ProcessWindowMessage=PASS' in windows['stdout'] and 'original_host_UI_script_cleanup=PASS' in windows['stdout'];assert not any(p.is_file() for p in (paths['windows'].parent/'runtime').rglob('*'))
 sdk=gates['sdk'];assert sdk['changed_cache_members']==['native_input_engine.cpp.obj'];assert members(paths['sdk'].parent/'libmatrixgame-native-cache.a')==sdk['cache_archive_members_sha256'];symbols=subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',str(paths['sdk'].parent/'matrixgame-rangers-input.elf')],text=True);assert not any(' U ' in line for line in symbols.splitlines()) and ' T RangersInputQueueSdkLinkRoot' in symbols
 old=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/result.json');check_hashes(old['all_link_inputs_sha256']);check_hashes(old['protected_previous_NRO_sha256'])
 for name,info in old['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 package=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/package-result.json');assert sha(ROOT/package['zip'])==package['zip_sha256'];check_hashes(package['files_sha256'],ROOT/package['package'])
 protected=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(protected[key])
 assert sha(Path(protected['primary_NRO']['path']))==protected['primary_NRO']['sha256'] and sha(ROOT/'Switch-E2E5-TEST.nro')==protected['root_NRO_sha256']
 for info in protected['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in protected['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PASS INPUT QUEUE/ENGINE ROUTING/REAL ORIGINAL GI DISPATCH/HOST SANITIZERS/ARM64 QEMU/SDK LINK','gates':records,'Windows_instrumentation':'PARTIAL MSVC ASAN','private_Windows_runtime_files':[],'protected_inputs_and_upstream_pins':'VERIFIED','E604_package_preserved_sha256':package['zip_sha256'],'native_UI_poll_owner':'NOT INTEGRATED','native_HID_runtime':'NOT RUN','full_native_Gov_entry':'NOT RUN','new_NRO':False,'commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status'])

if __name__=='__main__':main()

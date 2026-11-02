"""Audit native UI owner evidence without claiming physical HID/GPU execution."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();base=ROOT/'build/matrixgame/e2e610-native-ui-pump';paths={name:base/folder/'result.json' for name,folder in (('primitives','cpu/1'),('Windows','windows/6'),('host_owner','host-owner/4'),('ARM64_owner','arm64-owner/4'),('SDK','sdk/6'))};records={};gates={k:read(p) for k,p in paths.items()}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS') and all(c['exit']==0 for c in gate['commands']),name;check_hashes(gate['sources_sha256'])
  for key in ('reused_inputs_sha256','reused_objects_sha256','objects_sha256','dependency_objects_sha256'):
   if key in gate:check_hashes(gate[key])
  for path,info in gate.get('artifacts',{}).items():assert sha(ROOT/path)==info['sha256']
  snapshot=read(paths[name].parent/'source-snapshot.json');assert snapshot['result_sha256']==sha(paths[name]) and set(snapshot['files'])==set(gate['sources_sha256'])
  for source,info in snapshot['files'].items():assert info['sha256']==gate['sources_sha256'][source] and sha(ROOT/info['snapshot'])==info['sha256']
  records[name]={'path':str(paths[name].relative_to(ROOT)),'sha256':sha(paths[name]),'source_snapshot_sha256':sha(paths[name].parent/'source-snapshot.json'),'status':gate['status']}
  if name in ('Windows','host_owner','ARM64_owner'):
   log=(paths[name].parent/'runtime.log').read_text(encoding='utf-8');assert log==gate['stdout']
   for marker in ('actual_original_GR_WinMessage_GI=PASS','shared_key_cursor_imports=PASS','suspend_resume=PASS','nested_owner_queue_cancel=PASS','owner_physical_reset=PASS','original_host_UI_script_cleanup=PASS','scripted_backend_only=YES'):assert marker in log
   binary=paths[name].parent/('ui-context.exe' if name=='Windows' else 'ui-context');assert sha(binary)==gate['binary_sha256'];assert not any(p.is_file() for p in (paths[name].parent/'runtime').rglob('*'))
 sdk=gates['SDK'];assert len(sdk['archive_members_sha256'])==378;assert members(ROOT/sdk['archive_path'])==sdk['archive_members_sha256'];assert members(paths['SDK'].parent/'libmatrixgame-native-cache.a')==sdk['cache_members_sha256'];assert members(paths['SDK'].parent/'libmatrixgame-native-rangers-interface.a')==sdk['interface_members_sha256']
 assert sdk['changed_original_members']==['GR_Main.o','win32_compat_gdi.o','win32_compat_messages.o','win32_compat_window.o'];assert len(sdk['changed_cache_members'])==4
 symbols=subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',str(paths['SDK'].parent/'matrixgame-native-ui-pump.elf')],text=True);assert not any(' U ' in line for line in symbols.splitlines()) and all(root in symbols for root in sdk['retained_input_symbols'])
 production=paths['SDK'].parent/'GR_Main.cpp'
 for name in ('host_owner','ARM64_owner'):assert (paths[name].parent/'GR_Main.cpp').read_text(encoding='utf-8')==production.read_text(encoding='utf-8')
 old=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/result.json');check_hashes(old['all_link_inputs_sha256']);check_hashes(old['protected_previous_NRO_sha256'])
 for path,info in old['artifacts'].items():assert sha(ROOT/path)==info['sha256']
 package=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/package-result.json');assert sha(ROOT/package['zip'])==package['zip_sha256'];check_hashes(package['files_sha256'],ROOT/package['package'])
 protected=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(protected[key])
 assert sha(Path(protected['primary_NRO']['path']))==protected['primary_NRO']['sha256'] and sha(ROOT/'Switch-E2E5-TEST.nro')==protected['root_NRO_sha256']
 for info in protected['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in protected['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PASS PRODUCTION UI OWNER/ORIGINAL GI PUMP/SHARED KEY CURSOR IMPORTS/HOST SANITIZERS/ARM64/SDK LINK','gates':records,'production_GR_Main_comparison':'IDENTICAL UTF8 TEXT AFTER CRLF/LF NORMALIZATION; SEPARATE BYTE HASHES RETAINED','runtime_input_boundary':'SCRIPTED BACKEND; NO PHYSICAL HID OR SWITCHWINDOW RUNTIME CLAIM','Windows_instrumentation':'PARTIAL MSVC ASAN','protected_inputs_and_upstream_pins':'VERIFIED','private_runtime_files':[],'E604_package_preserved_sha256':package['zip_sha256'],'native_GPU_HID_runtime':'NOT RUN','full_native_Gov_entry':'NOT RUN','new_NRO':False,'commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status'])

if __name__=='__main__':main()

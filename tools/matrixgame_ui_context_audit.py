"""Audit the original UI context, rollback and preserved earlier packages."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();base=ROOT/'build/matrixgame/e2e607-ui-context';paths={name:base/folder/'result.json' for name,folder in (('host','host/2'),('ARM64','arm64/1'),('Windows','windows/2'),('SDK','sdk/1'))};gates={k:read(p) for k,p in paths.items()};records={}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS') and all(c['exit']==0 for c in gate['commands']),name;check_hashes(gate['sources_sha256'])
  for key in ('reused_objects_sha256','reused_inputs_sha256','objects_sha256','dependency_objects_sha256'):
   if key in gate:check_hashes(gate[key])
  records[name]={'path':str(paths[name].relative_to(ROOT)),'sha256':sha(paths[name]),'status':gate['status']}
  if name!='SDK':
   log=(paths[name].parent/'runtime.log').read_text(encoding='utf-8');assert log==gate['stdout'];assert log.count('original_CFG_TCC_canvas_palette_cursors=PASS original_script_global_UI=PASS')==2 and 'cleanup=PASS' in log
   if name!='Windows':assert 'actual_late_realloc_failure=PASS three_real_buffers_released=PASS globals_restored=PASS' in log
   binary=paths[name].parent/('ui-context.exe' if name=='Windows' else 'ui-context');assert sha(binary)==gate['binary_sha256']
   files=[p for p in (paths[name].parent/'runtime').rglob('*') if p.is_file()];assert not files,(name,files);records[name]['private_runtime_files']=[]
 sdk=gates['SDK'];assert sdk['changed_members']==['renderer_platform.o'] and len(sdk['archive_members_sha256'])==378;assert members(ROOT/sdk['archive_path'])==sdk['archive_members_sha256']
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 symbols=subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',str(paths['SDK'].parent/'matrixgame-ui-context.elf')],text=True);assert not any(' U ' in line for line in symbols.splitlines()) and all(root in symbols for root in sdk['retained_UI_symbols'])
 old=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/result.json');check_hashes(old['all_link_inputs_sha256']);check_hashes(old['protected_previous_NRO_sha256'])
 for name,info in old['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 package=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/package-result.json');assert sha(ROOT/package['zip'])==package['zip_sha256'];check_hashes(package['files_sha256'],ROOT/package['package'])
 prior=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json');check_hashes(prior['protected_game_sha256']);check_hashes(prior['protected_frozen_sha256']);git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in prior['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS ORIGINAL READONLY UI CONTEXT/REAL LATE ALLOCATION ROLLBACK/HOST SANITIZERS/WINDOWS/ARM64/SDK LINK','gates':records,'GPU_runtime':'NOT RUN','full_native_Gov_entry':'NOT RUN','new_NRO':False,'E604_package_preserved_sha256':package['zip_sha256'],'protected_inputs_and_upstream_pins':'VERIFIED','commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status'])

if __name__=='__main__':main()

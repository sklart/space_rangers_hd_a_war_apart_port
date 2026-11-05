"""Fresh ordinary diagnostic NRO targets; preserve all previous packages and inputs."""
import argparse,json,os,re,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_sdk_link_provenance import deko_link_provenance
from matrixgame_optimization_provenance import optimized_compiler_provenance

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output-root',type=Path,default=ROOT/'build/matrixgame/e2e604-current-diagnostic');args=parser.parse_args()
 parent=args.output_root.resolve();assert parent.is_relative_to(ROOT/'build');parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir()
 candidates=[p for p in (ROOT/'build/matrixgame/e2e610-native-ui-pump/sdk').glob('*/result.json') if read(p)['status'].startswith('PASS') and 'dependency_objects_sha256' in read(p)]
 assert candidates,'build the current verified native UI dependency with matrixgame_native_ui_pump_sdk.py first'
 dependency_path=max(candidates,key=lambda p:p.stat().st_mtime_ns);dependency=read(dependency_path);check_hashes(dependency['dependency_objects_sha256']);archive=ROOT/dependency['archive_path'];assert sha(archive)==dependency['dependency_archive_sha256']
 prior=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json')
 protected={str(Path(prior['primary_NRO']['path'])):prior['primary_NRO']['sha256'],str(ROOT/'Switch-E2E5-TEST.nro'):prior['root_NRO_sha256']};protected.update({str(ROOT/info['path']):info['sha256'] for info in prior['preserved_E590_packages'].values()});check_hashes(protected)
 baseline=ROOT/'build/matrixgame/e2e601-revolution-seed/sdk';reference=read(max(baseline.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));command=reference['commands'][0]['command'][:];command[command.index('-B')+1]=str(out)
 command=[('-DBRIDGE_RANGERS_HOST_ARCHIVE='+str(archive)) if s.startswith('-DBRIDGE_RANGERS_HOST_ARCHIVE=') else ('-DMATRIXGAME_RANGERS_DEPENDENCY_MANIFEST='+str(dependency_path)) if s.startswith('-DMATRIXGAME_RANGERS_DEPENDENCY_MANIFEST=') else s for s in command]
 command=[s for s in command if not s.startswith(('-DCMAKE_BUILD_TYPE=','-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=','-DCMAKE_C_FLAGS_RELWITHDEBINFO=','-DCMAKE_EXPORT_COMPILE_COMMANDS='))]
 command+=['-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O2 -g -UNDEBUG -ffp-contract=off','-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O2 -g -UNDEBUG -ffp-contract=off','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','hardware':'HARDWARE_PENDING','scope':'standalone original native Init/Draw/manual diagnostic; integrated Galactic Gov/FRun not invoked by this entry','dependency_result_path':str(dependency_path.relative_to(ROOT)),'dependency_result_sha256':sha(dependency_path),'protected_previous_NRO_sha256':protected,'new_NRO':False}
 def run(cmd,label):
  cmd=list(map(str,cmd));path=out/(label+'.log')
  with path.open('w',encoding='utf-8') as stream:p=subprocess.run(cmd,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT)
  log=path.read_text(encoding='utf-8',errors='replace');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(label,p.returncode,log[-6500:] if p.returncode else '',flush=True);assert p.returncode==0,label;return log
 try:
  run(command,'configure');result['compiler_provenance']=optimized_compiler_provenance(out)
  run([command[0],'--build',out,'--target','matrixgame-native-interactive-nro','matrixgame-native-diagnostic-nro','-j','4'],'build-standard-NRO-targets')
  metadata=read(out/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],out/'source');check_hashes(metadata['rangers_host']['archives_sha256']);assert metadata['rangers_host']['dependency_manifest_sha256']==sha(dependency_path)
  original_baseline=read(baseline/'native-diagnostic-build.json')['generated_original_sha256']
  generated=metadata['generated_original_sha256'];assert generated.keys()==original_baseline.keys()
  changed={name for name in generated if generated[name]!=original_baseline[name]}
  assert changed=={'MatrixLib/3G/OriginalManagedTexture.cpp','MatrixLib/3G/Texture.cpp','MatrixGame/src/OriginalTransition.cpp'},changed
  result['generated_original_changes_vs_E601']=sorted(changed)
  artifacts={};variants={};fonts=[out/'debug-font/original-font.bin',*[out/f'text-font/font{i}.bin' for i in range(4)]]
  for entry,folder in (('matrixgame-native-interactive','interactive-nro'),('matrixgame-native-diagnostic','diagnostic-nro')):
   elf=out/(entry+'.elf');nro=out/folder/(entry+'.nro');nacp=out/folder/'control.nacp';symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],entry+'-symbols');assert not any(' U ' in line for line in symbols.splitlines())
   for name in ('CGame::Init','CGame::Deinit','CMatrixMap::Draw','RangersHostSession'):assert name in symbols,name
   header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],entry+'-header');assert 'AArch64' in header
   for font in fonts:assert font.read_bytes() in elf.read_bytes(),str(font)
   notes=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-n',elf],entry+'-build-id');elf_id=re.search(r'Build ID: ([0-9a-f]+)',notes).group(1);nro_id=nro.read_bytes()[0x40:0x60].hex();assert nro_id==elf_id.ljust(64,'0')
   link_map=out/('hosted-interactive.map' if entry.endswith('interactive') else 'hosted-diagnostic.map')
   provenance=deko_link_provenance(link_map,metadata['deko_library'])
   if metadata['deko_library']=='deko3dd':
    assert 'dk::detail::SetContextForDebug(' in symbols and 'dk::detail::RaiseError(DkResult, char const*)' in symbols
    assert 'dk::detail::RaiseError(DkResult)\n' not in symbols
   title='MatrixGame r37 Interactive' if entry.endswith('interactive') else 'MatrixGame r37 Quick Smoke'
   variants[entry]={'elf_build_id':elf_id,'nro_build_id':nro_id,'NRO':str(nro.relative_to(ROOT)),'nacp_title':title,'validation':verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes(),title),'strong_undefined':[],'runtime':'NOT RUN'}
   variants[entry]['deko_link_provenance']=provenance
   artifacts.update({str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (elf,nro,nacp)})
  check_hashes(protected);check_hashes(prior['protected_game_sha256']);check_hashes(prior['protected_frozen_sha256']);assert sha(archive)==dependency['dependency_archive_sha256']
  result.update(status='PASS TWO SEPARATE STANDARD LIBNX/DEKO3D SDK NRO TARGETS; PACKAGE/LINK ONLY; HARDWARE_PENDING',new_NRO=True,variants=variants,artifacts=artifacts,metadata_sha256=sha(out/'native-diagnostic-build.json'),embedded_fonts_sha256={str(p.relative_to(ROOT)):sha(p) for p in fonts},all_link_inputs_sha256={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.a','.o','.obj')})
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

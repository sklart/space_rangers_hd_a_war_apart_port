"""Compile the real deko RGB565 presenter and retain it in an actual SDK ELF."""
import json,os,shlex,shutil,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parent=ROOT/'build/matrixgame/e2e605-rangers-canvas/sdk';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 reference=ROOT/'build/matrixgame/e2e604-current-diagnostic/1';old_dependency=ROOT/'build/matrixgame/e2e603-campaign-exception/sdk/1';prior=read(old_dependency/'result.json');assert prior['status'].startswith('PASS');old_archive=ROOT/prior['archive_path'];assert sha(old_archive)==prior['dependency_archive_sha256']
 cache=reference/'libmatrixgame-native-cache.a';assert cache.is_file();saved=read(reference/'result.json');check_hashes(saved['all_link_inputs_sha256']);check_hashes(saved['protected_previous_NRO_sha256'])
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','native_GPU_runtime':'NOT RUN','new_NRO':False,'sources_sha256':{}}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-5500:] if p.returncode else '',flush=True);assert p.returncode==0,label;return log
 try:
  entries=read(reference/'compile_commands.json');entry=next(v for v in entries if v['file'].replace('\\','/').endswith('/native_diagnostic_smoke.cpp'));command=[s.strip('"') for s in shlex.split(entry['command'],posix=False)]
  provider=ROOT/'port/matrixgame/native-engine/native_rangers_canvas.cpp';obj=out/'native_rangers_canvas.o';command[command.index('-c')+1]=str(provider);command[command.index('-o')+1]=str(obj);run(command,'compile-native-canvas',entry['directory'])
  interface_source=ROOT/'port/matrixgame/native-engine/native_rangers_interface.cpp';entry=next(v for v in entries if v['file'].replace('\\','/').endswith('/native_rangers_interface.cpp'));command=[s.strip('"') for s in shlex.split(entry['command'],posix=False)];interface_object=out/'native_rangers_interface.cpp.obj';command[command.index('-o')+1]=str(interface_object);run(command,'compile-native-interface-owner',entry['directory'])
  old_interface=ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk/libmatrixgame-native-rangers-interface.a';interface_archive=out/'libmatrixgame-native-rangers-interface.a';shutil.copyfile(old_interface,interface_archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',interface_archive,interface_object],'replace-interface-owner');interface_before=members(old_interface);interface_after=members(interface_archive);assert set(interface_before)==set(interface_after) and [k for k in interface_before if interface_before[k]!=interface_after[k]]==['native_rangers_interface.cpp.obj']
  flags=read(ROOT/'build/matrixgame/e2e572-rangers-host-sdk/fresh/platform/renderer_platform.build.json')['inputs']['flags'];source=ROOT/'port/switch/platform/renderer_platform.cpp';renderer=out/'renderer_platform.o';run([flags['compiler'],*flags['flags'],'-c',source,'-o',renderer],'compile-original-canvas-dispatch')
  archive=out/'liboriginal-rangers-host-canvas.a';shutil.copyfile(old_archive,archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,renderer],'replace-renderer-member');before=members(old_archive);after=members(archive);assert set(before)==set(after);changed=sorted(k for k in before if before[k]!=after[k]);assert changed==['renderer_platform.o']
  nm='C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe';symbols=run([nm,'-g','--defined-only',obj],'presenter-symbols');roots=sorted({line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2]=='T' and 'NativeRangersCanvas' in line.split()[-1]});assert roots
  command=next(c['command'] for c in prior['commands'] if c['label']=='link-retained-campaign-API')[:];elf=out/'matrixgame-rangers-canvas.elf';command[command.index('-o')+1]=str(elf)
  assert command.count(str(old_archive))==1;count=sum(Path(s.replace('\\','/')).name=='libmatrixgame-native-cache.a' for s in command);assert count>=1
  assert command.count(str(old_interface))==1
  command=[str(archive) if s==str(old_archive) else str(interface_archive) if s==str(old_interface) else str(cache) if Path(s.replace('\\','/')).name=='libmatrixgame-native-cache.a' else ('-Wl,-Map='+str(out/'rangers-canvas.map')) if s.startswith('-Wl,-Map=') else s for s in command]
  command.insert(command.index('-Wl,--start-group'),str(obj));command+=['-Wl,--undefined='+root for root in roots];run(command,'link-retained-presenter-and-campaign-API',ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk')
  linked=run([nm,'-g',elf],'elf-symbols');assert all(root in linked for root in roots);assert not any(' U ' in line for line in linked.splitlines())
  header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header');assert 'AArch64' in header
  sources=[provider,source,interface_source,ROOT/'port/matrixgame/native-engine/native_rangers_interface.hpp',ROOT/'port/switch/platform/rgb565_presentation.hpp',ROOT/'port/matrixgame/native-engine/native_rangers_canvas.hpp',ROOT/'port/matrixgame/native-engine/rangers_canvas_pixels.hpp',ROOT/'port/matrixgame/native-engine/CMakeLists.txt',ROOT/'tests/test_matrixgame_rangers_canvas.cpp',Path(__file__).resolve()]
  dependency_objects={k:v for k,v in prior['dependency_objects_sha256'].items() if Path(k).name!='renderer_platform.o'};dependency_objects[str(renderer.relative_to(ROOT))]=sha(renderer);check_hashes(dependency_objects)
  result.update(status='PASS REAL DEKO RGB565 PRESENTER/NATIVE INTERFACE OWNER/ORIGINAL DISPATCH COMPILE/RETAINED SDK LINK; ZERO STRONG UNDEFINED; GPU NOT RUN',retained_presenter_symbols=roots,changed_members=changed,archive_members_sha256=after,dependency_archive_sha256=sha(archive),archive_path=str(archive.relative_to(ROOT)),interface_archive_members_sha256=interface_after,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in sources},artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (obj,renderer,archive,interface_object,interface_archive,elf,out/'rangers-canvas.map')},preserved_E604_link_inputs_sha256=saved['all_link_inputs_sha256'],preserved_previous_NRO_sha256=saved['protected_previous_NRO_sha256'])
  check_hashes(result['sources_sha256']);check_hashes(saved['all_link_inputs_sha256']);check_hashes(saved['protected_previous_NRO_sha256']);assert sha(old_archive)==prior['dependency_archive_sha256']
  result['dependency_objects_sha256']=dependency_objects
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

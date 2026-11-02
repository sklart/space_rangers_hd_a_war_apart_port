"""Production Robot caller overlay + real native window/device owner SDK closure."""
import json,os,shlex,shutil,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
from e2e4_script_width_overrides import prepare_script_source_overlay
from e2e_source_overrides import generated_source

def main():
 parent=ROOT/'build/matrixgame/e2e606-caller-presentation/sdk';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();reference=ROOT/'build/matrixgame/e2e604-current-diagnostic/1';old=ROOT/'build/matrixgame/e2e605-rangers-canvas/sdk/4';prior=read(old/'result.json');assert prior['status'].startswith('PASS');check_hashes(prior['dependency_objects_sha256']);old_archive=ROOT/prior['archive_path'];assert sha(old_archive)==prior['dependency_archive_sha256'];old_interface=old/'libmatrixgame-native-rangers-interface.a'
 saved=read(reference/'result.json');check_hashes(saved['all_link_inputs_sha256']);check_hashes(saved['protected_previous_NRO_sha256']);entries=read(reference/'compile_commands.json');env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','native_GPU_runtime':'NOT RUN','new_NRO':False}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-6000:] if p.returncode else '',flush=True);assert p.returncode==0,label;return log
 try:
  header=ROOT/'port/switch/platform/planet_battle_presentation.hpp';caller=ROOT/'port/matrixgame/native-engine/rangers_caller_presentation.cpp';interface=ROOT/'port/matrixgame/native-engine/native_rangers_interface.cpp';objects=[]
  entry=next(v for v in entries if v['file'].replace('\\','/').endswith('/native_rangers_interface.cpp'))
  for source in (interface,caller):
   command=[s.strip('"') for s in shlex.split(entry['command'],posix=False)];obj=out/(source.name+'.obj');command[command.index('-c')+1]=str(source);command[command.index('-o')+1]=str(obj);run(command,'compile-'+source.stem,entry['directory']);objects.append(obj)
  interface_archive=out/'libmatrixgame-native-rangers-interface.a';shutil.copyfile(old_interface,interface_archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',interface_archive,*objects],'integrate-caller-owner')
  before_interface=members(old_interface);after_interface=members(interface_archive);assert set(after_interface)==set(before_interface)|{'rangers_caller_presentation.cpp.obj'};assert after_interface['native_rangers_support.cpp.obj']==before_interface['native_rangers_support.cpp.obj']
  overlay=out/'source-overlay';prepare_script_source_overlay(ROOT/'upstream/cpp/src',overlay);source=out/'Robot.cpp';assert generated_source(overlay/'Robot.cpp',source,audio_on=True,e2e5_profile=True,script_width=True)==source
  text=source.read_text(encoding='utf-8');assert 'native_caller->EnterBattle()' in text and 'native_caller->Restore()' in text and 'native_run_error' in text
  flags=read(ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt');robot=out/'Robot.o';run([flags['compiler'],*flags['flags'],'-c',source,'-o',robot],'compile-production-Robot')
  archive=out/'liboriginal-rangers-host-caller.a';shutil.copyfile(old_archive,archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,robot],'replace-original-Robot');before=members(old_archive);after=members(archive);assert set(before)==set(after);assert sorted(k for k in before if before[k]!=after[k])==['Robot.o']
  command=next(c['command'] for c in prior['commands'] if c['label']=='link-retained-presenter-and-campaign-API')[:];elf=out/'matrixgame-caller-presentation.elf';command[command.index('-o')+1]=str(elf);assert command.count(str(old_archive))==1 and command.count(str(old_interface))==1
  command=[str(archive) if s==str(old_archive) else str(interface_archive) if s==str(old_interface) else ('-Wl,-Map='+str(out/'caller-presentation.map')) if s.startswith('-Wl,-Map=') else s for s in command];run(command,'link-retained-native-caller-and-campaign',ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk')
  nm='C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe';symbols=run([nm,'-g',elf],'symbols');assert not any(' U ' in line for line in symbols.splitlines());assert 'RangersCallerPresentation' in symbols and 'NativeRangersCanvas' in symbols
  elf_header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'header');assert 'AArch64' in elf_header
  sources=[header,caller,interface,source,ROOT/'port/matrixgame/native-engine/rangers_caller_presentation.hpp',ROOT/'port/matrixgame/native-engine/CMakeLists.txt',ROOT/'tools/matrixgame_campaign_caller_overrides.py',ROOT/'tools/e2e_source_overrides.py',ROOT/'tools/e2e5_host_game_build.py',Path(__file__).resolve()];hashes={k:v for k,v in prior['dependency_objects_sha256'].items() if Path(k).name!='Robot.o'};hashes[str(robot.relative_to(ROOT))]=sha(robot);check_hashes(hashes)
  result.update(status='PASS PRODUCTION FRUN CALLER DISPATCH/NATIVE OWNER SDK COMPILE/RETAINED LINK; ZERO STRONG UNDEFINED; NOT RUN',sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in sources},dependency_objects_sha256=hashes,archive_path=str(archive.relative_to(ROOT)),dependency_archive_sha256=sha(archive),archive_members_sha256=after,changed_members=['Robot.o'],native_interface_archive_members_sha256=after_interface,artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (*objects,robot,archive,interface_archive,elf,out/'caller-presentation.map')},preserved_E604_link_inputs_sha256=saved['all_link_inputs_sha256'],preserved_previous_NRO_sha256=saved['protected_previous_NRO_sha256'])
  check_hashes(result['sources_sha256']);check_hashes(saved['all_link_inputs_sha256']);check_hashes(saved['protected_previous_NRO_sha256']);assert members(old_archive)==before and members(old_interface)==before_interface
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

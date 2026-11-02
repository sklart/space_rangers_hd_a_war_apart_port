"""Compile current HID routing and input queue, retain the real SDK closure."""
import json,os,shlex,shutil,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parent=ROOT/'build/matrixgame/e2e609-rangers-input/sdk';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();reference=ROOT/'build/matrixgame/e2e604-current-diagnostic/1';saved=read(reference/'result.json');check_hashes(saved['all_link_inputs_sha256']);check_hashes(saved['protected_previous_NRO_sha256'])
 prior=read(ROOT/'build/matrixgame/e2e607-ui-context/sdk/1/result.json');assert prior['status'].startswith('PASS');check_hashes(prior['dependency_objects_sha256'])
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','new_NRO':False,'native_UI_poll_owner':'NOT INTEGRATED'}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-6500:] if p.returncode else '',flush=True);assert p.returncode==0,label;return log
 try:
  entry=next(e for e in read(reference/'compile_commands.json') if e['file'].replace('\\','/').endswith('/native_input_engine.cpp'));base=[s.strip('"') for s in shlex.split(entry['command'],posix=False)]
  sources=[ROOT/'port/matrixgame/native-engine/native_input_engine.cpp',ROOT/'tests/probe_matrixgame_rangers_input_sdk.cpp'];objects=[]
  for source in sources:
   obj=out/('native_input_engine.cpp.obj' if source==sources[0] else 'queue-probe.o');command=base[:];command[command.index('-c')+1]=str(source);command[command.index('-o')+1]=str(obj);run(command,'compile-'+source.stem,entry['directory']);objects.append(obj)
  old_cache=reference/'libmatrixgame-native-cache.a';cache=out/'libmatrixgame-native-cache.a';shutil.copyfile(old_cache,cache);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',cache,objects[0]],'replace-engine-input');before=members(old_cache);after=members(cache);assert set(before)==set(after) and [k for k in before if before[k]!=after[k]]==['native_input_engine.cpp.obj']
  command=next(c['command'] for c in prior['commands'] if c['label']=='link-retained-ui-caller-campaign')[:];elf=out/'matrixgame-rangers-input.elf';command[command.index('-o')+1]=str(elf);assert any(Path(s.replace('\\','/')).name=='libmatrixgame-native-cache.a' for s in command)
  command=[str(cache) if Path(s.replace('\\','/')).name=='libmatrixgame-native-cache.a' else ('-Wl,-Map='+str(out/'rangers-input.map')) if s.startswith('-Wl,-Map=') else s for s in command];command.insert(command.index('-Wl,--start-group'),str(objects[1]));command+=['-Wl,--undefined=RangersInputQueueSdkLinkRoot'];run(command,'link-retained-ui-input-caller-campaign',ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk')
  symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',elf],'symbols');assert not any(' U ' in line for line in symbols.splitlines()) and ' T RangersInputQueueSdkLinkRoot' in symbols
  header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'header');assert 'AArch64' in header
  sources += [ROOT/'port/matrixgame/native-engine'/name for name in ('native_ui_target.hpp','rangers_input_queue.hpp','native_input.hpp','native_input_engine.hpp','rangers_canvas_pixels.hpp')];sources.append(Path(__file__).resolve())
  result.update(status='PASS SDK CURRENT ENGINE INPUT/QUEUE COMPILE/RETAINED UI-CALLER-CAMPAIGN LINK; ZERO STRONG UNDEFINED; NOT RUN',sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in sources},changed_cache_members=['native_input_engine.cpp.obj'],cache_archive_members_sha256=after,artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (*objects,cache,elf,out/'rangers-input.map')},preserved_E604_link_inputs_sha256=saved['all_link_inputs_sha256'],preserved_previous_NRO_sha256=saved['protected_previous_NRO_sha256'],prior_result_sha256=sha(ROOT/'build/matrixgame/e2e607-ui-context/sdk/1/result.json'))
  check_hashes(result['sources_sha256']);check_hashes(saved['all_link_inputs_sha256']);check_hashes(saved['protected_previous_NRO_sha256'])
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

"""Compile production exception overlays and link against the retained SDK closure."""
import json,os,shutil,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
from e2e4_script_width_overrides import prepare_script_source_overlay
from e2e_source_overrides import generated_source

def main():
 parent=ROOT/'build/matrixgame/e2e603-campaign-exception/sdk';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();overlay=out/'source-overlay';prepare_script_source_overlay(ROOT/'upstream/cpp/src',overlay)
 prior_path=ROOT/'build/matrixgame/e2e601-revolution-seed/sdk-dependency/result.json';prior=read(prior_path);assert prior['status'].startswith('PASS');old=ROOT/prior['archive_path'];assert sha(old)==prior['dependency_archive_sha256']
 base=ROOT/'build/matrixgame/e2e601-revolution-seed/sdk';closure=read(max(base.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));assert closure['status'].startswith('PASS');check_hashes(closure['new_link_inputs_sha256']);check_hashes(closure['reused_E592_link_inputs_sha256'])
 flags=read(ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt');env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 result={'status':'INCOMPLETE','commands':[],'runtime':'NOT RUN','Switch':'NOT RUN','new_NRO':False,'sources_sha256':{},'production_generated':[]}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-6000:] if p.returncode else '',flush=True);assert p.returncode==0,label;return log
 try:
  objects=[]
  for name in ('Robot.cpp','fGov.cpp'):
   source=out/name;actual=generated_source(overlay/name,source,audio_on=True,e2e5_profile=True,script_width=True);assert actual==source
   text=source.read_text(encoding='utf-8');assert ('native_run_error' if name=='Robot.cpp' else 'native_battle_error') in text
   obj=out/(source.stem+'.o');run([flags['compiler'],*flags['flags'],'-c',source,'-o',obj],'compile-'+source.stem);objects.append(obj);result['production_generated'].append(name)
   for p in (source,overlay/name,ROOT/'upstream/cpp/src'/name):result['sources_sha256'][str(p.relative_to(ROOT))]=sha(p)
  archive=out/'liboriginal-rangers-host-exception.a';shutil.copyfile(old,archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,*objects],'replace-two-members')
  before=members(old);after=members(archive);assert set(before)==set(after);changed=sorted(k for k in before if before[k]!=after[k]);assert changed==['Robot.o','fGov.o']
  command=next(c['command'] for c in closure['commands'] if c['label']=='link-retained-campaign-API')[:];elf=out/'matrixgame-campaign-exception.elf';command[command.index('-o')+1]=str(elf)
  count=command.count(str(old));assert count==1,count;command=[str(archive) if s==str(old) else ('-Wl,-Map='+str(out/'campaign-exception.map')) if s.startswith('-Wl,-Map=') else s for s in command]
  run(command,'link-retained-campaign-API',ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk')
  symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',elf],'symbols');assert not any(' U ' in line for line in symbols.splitlines());assert all(name in symbols for name in closure['retained_API_symbols'])
  header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'header');assert 'AArch64' in header
  for p in (ROOT/'tools/e2e_source_overrides.py',ROOT/'tools/e2e5_host_game_build.py',ROOT/'tools/matrixgame_campaign_exception_overrides.py',Path(__file__).resolve()):result['sources_sha256'][str(p.relative_to(ROOT))]=sha(p)
  check_hashes(result['sources_sha256']);assert sha(old)==prior['dependency_archive_sha256'];check_hashes(closure['new_link_inputs_sha256']);check_hashes(closure['reused_E592_link_inputs_sha256'])
  hashes={k:v for k,v in prior['dependency_objects_sha256'].items() if Path(k).name not in changed};hashes.update({str(p.relative_to(ROOT)):sha(p) for p in objects});check_hashes(hashes)
  result.update(status='PASS TWO PRODUCTION GENERATED SDK UNITS/ARCHIVE/RETAINED CAMPAIGN API LINK; ZERO STRONG UNDEFINED; NOT RUN',changed_members=changed,archive_path=str(archive.relative_to(ROOT)),dependency_archive_sha256=sha(archive),archive_members_sha256=after,dependency_objects_sha256=hashes,artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (archive,elf,out/'campaign-exception.map')},reused_closure_result_sha256=sha(max(base.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns)))
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

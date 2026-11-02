"""Replace only original fGameSettings.o in a separate SDK dependency archive."""
import json,os,shutil,subprocess
from pathlib import Path
from matrixgame_campaign_overrides import apply
from matrixgame_legacy_wav_sdk_dependency import members
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e598-campaign/sdk-dependency'
def main():
 BASE.mkdir(parents=True,exist_ok=True);prior=read(ROOT/'build/matrixgame/e2e590-completed-battle/sdk-dependency/result.json');assert prior['status'].startswith('PASS');check_hashes(prior['dependency_objects_sha256'])
 old=ROOT/prior['archive_path'];assert sha(old)==prior['dependency_archive_sha256']
 flags=read(ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt');mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json');original=ROOT/mapping['upstream/cpp/src/fGameSettings.cpp']
 source=BASE/'fGameSettings.cpp';rendered=apply(original.read_text(encoding='utf-8'))
 if source.exists():assert source.read_text(encoding='utf-8')==rendered
 else:source.write_text(rendered,encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','runtime':'NOT RUN'};env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,(p.stdout+p.stderr)[-5000:] if p.returncode else '',flush=True);assert not p.returncode
 try:
  obj=BASE/'fGameSettings.o';run([flags['compiler'],*flags['flags'],'-c',source,'-o',obj],'compile-original-campaign-generator')
  archive=BASE/'liboriginal-rangers-host-campaign.a';assert not archive.exists();shutil.copyfile(old,archive)
  run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,obj],'replace-one-archive-member')
  before=members(old);after=members(archive);assert set(before)==set(after)
  changed=[name for name in before if before[name]!=after[name]];assert changed==['fGameSettings.o'] and after['fGameSettings.o']==sha(obj)
  objects=dict(prior['dependency_objects_sha256']);key=next(k for k in objects if Path(k).name=='fGameSettings.o');del objects[key];objects[str(obj.relative_to(ROOT))]=sha(obj)
  check_hashes(objects);assert sha(old)==prior['dependency_archive_sha256']
  result.update(status='PASS; SDK ORIGINAL CAMPAIGN GENERATOR COMPILE; ONE OF 378 MEMBERS REPLACED; NOT RUN',dependency_objects_sha256=objects,dependency_archive_sha256=sha(archive),archive_path=str(archive.relative_to(ROOT)),old_archive_sha256=sha(old),archive_members_sha256=after,changed_members=changed,source_sha256={str(p.relative_to(ROOT)):sha(p) for p in (source,original,ROOT/'tools/matrixgame_campaign_overrides.py',ROOT/'tools/e2e_source_overrides.py',Path(__file__).resolve())})
 finally:
  path=BASE/'result.json';i=1
  while path.exists():path=BASE/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

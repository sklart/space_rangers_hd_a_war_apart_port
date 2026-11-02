"""Separate SDK archive and standard campaign target for verified cleanup overlays."""
import json, os, shutil, subprocess, sys
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
from matrixgame_campaign_cleanup_checks import BASE
from matrixgame_campaign_cleanup_cross import UNITS
from matrixgame_campaign_cleanup_overrides import apply

def main():
 out=BASE/'sdk-dependency';out.mkdir(parents=True,exist_ok=True)
 prior_path=ROOT/'build/matrixgame/e2e598-campaign/sdk-dependency/result.json';prior=read(prior_path);assert prior['status'].startswith('PASS')
 check_hashes(prior['dependency_objects_sha256']);old=ROOT/prior['archive_path'];assert sha(old)==prior['dependency_archive_sha256']
 archive=out/'liboriginal-rangers-host-campaign.a';assert not archive.exists(),'preserve existing dependency archive'
 flags=read(ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt');mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json')
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','runtime':'NOT RUN','source_sha256':{},'prior_result_sha256':sha(prior_path)}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-6500:] if p.returncode else '',flush=True);assert p.returncode==0,label
 try:
  objects=[]
  for name in UNITS:
   original=ROOT/mapping['upstream/cpp/src/'+name];source=out/name;assert not source.exists();source.write_text(apply(name,original.read_text(encoding='utf-8')),encoding='utf-8')
   obj=out/(source.stem+'.o');run([flags['compiler'],*flags['flags'],'-c',source,'-o',obj],'compile-'+source.stem);objects.append(obj)
   for p in (original,source):result['source_sha256'][str(p.relative_to(ROOT))]=sha(p)
  shutil.copyfile(old,archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,*objects],'replace-seven-archive-members')
  before=members(old);after=members(archive);assert set(before)==set(after)
  changed=sorted(name for name in before if before[name]!=after[name]);assert changed==sorted(p.name for p in objects),changed
  dep_objects={k:v for k,v in prior['dependency_objects_sha256'].items() if Path(k).name not in changed}
  dep_objects.update({str(p.relative_to(ROOT)):sha(p) for p in objects});check_hashes(dep_objects)
  assert sha(old)==prior['dependency_archive_sha256']
  for p in (ROOT/'tools/matrixgame_campaign_cleanup_overrides.py',ROOT/'tools/e2e_source_overrides.py',ROOT/'port/switch/platform/script_array_lifetime.hpp',Path(__file__).resolve()):result['source_sha256'][str(p.relative_to(ROOT))]=sha(p)
  result.update(status='PASS; SDK SEVEN CAMPAIGN CLEANUP TUS; SEPARATE ARCHIVE; NOT RUN',dependency_objects_sha256=dep_objects,dependency_archive_sha256=sha(archive),archive_path=str(archive.relative_to(ROOT)),old_archive_sha256=sha(old),archive_members_sha256=after,changed_members=changed)
 finally:
  path=out/'result.json';assert not path.exists();path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 text=(ROOT/'build/matrixgame/e2e598-campaign/fixture/sdk-closure.py').read_text(encoding='utf-8')
 assert 'e2e598-campaign' in text;text=text.replace('e2e598-campaign','e2e599-campaign-cleanup')
 fixture=BASE/'fixture';fixture.mkdir(exist_ok=True);script=fixture/'sdk-closure.py';assert not script.exists();script.write_text(text,encoding='utf-8')
 check_hashes(result['source_sha256']);subprocess.run([sys.executable,script],check=True)

if __name__=='__main__':main()

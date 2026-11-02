"""Integrate the verified revolution seed in one original SDK member and standard targets."""
import json,os,shutil,subprocess,sys
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
from matrixgame_campaign_cleanup_overrides import apply as cleanup
from matrixgame_campaign_snapshot_overrides import apply as snapshot_cleanup
from matrixgame_campaign_revolution_overrides import apply as revolution

def main():
 base=ROOT/'build/matrixgame/e2e601-revolution-seed';out=base/'sdk-dependency';out.mkdir(parents=True,exist_ok=True)
 candidate=read(base/'1/result.json');assert candidate['status'].startswith('PASS');check_hashes(candidate['sources_sha256']);check_hashes(candidate['reused_objects_sha256'])
 prior_path=ROOT/'build/matrixgame/e2e600-campaign-navigation/sdk-dependency/result.json';prior=read(prior_path);assert prior['status'].startswith('PASS');check_hashes(prior['dependency_objects_sha256']);old=ROOT/prior['archive_path'];assert sha(old)==prior['dependency_archive_sha256']
 archive=out/'liboriginal-rangers-host-revolution.a';assert not archive.exists();flags=read(ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt');mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json');original=ROOT/mapping['upstream/cpp/src/aPlanet.cpp'];source=out/'aPlanet.cpp';assert not source.exists();source.write_text(revolution('aPlanet.cpp',snapshot_cleanup('aPlanet.cpp',cleanup('aPlanet.cpp',original.read_text(encoding='utf-8')))),encoding='utf-8')
 assert source.read_text(encoding='utf-8')==(base/'1/aPlanet.cpp').read_text(encoding='utf-8')
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','runtime':'NOT RUN','prior_result_sha256':sha(prior_path),'source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in (source,original,ROOT/'tools/e2e_source_overrides.py',ROOT/'tools/matrixgame_campaign_revolution_overrides.py',Path(__file__).resolve())}}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-6000:] if p.returncode else '',flush=True);assert p.returncode==0,label
 try:
  obj=out/'aPlanet.o';run([flags['compiler'],*flags['flags'],'-c',source,'-o',obj],'compile-aPlanet');shutil.copyfile(old,archive);run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,obj],'replace-one-member')
  before=members(old);after=members(archive);assert set(before)==set(after);changed=sorted(n for n in before if before[n]!=after[n]);assert changed==['aPlanet.o'] and after['aPlanet.o']==sha(obj)
  hashes={k:v for k,v in prior['dependency_objects_sha256'].items() if Path(k).name!='aPlanet.o'};hashes[str(obj.relative_to(ROOT))]=sha(obj);check_hashes(hashes);assert sha(old)==prior['dependency_archive_sha256']
  result.update(status='PASS; SDK ORIGINAL REVOLUTION LOW32 SEED; ONE OF 378 MEMBERS REPLACED; NOT RUN',dependency_objects_sha256=hashes,dependency_archive_sha256=sha(archive),archive_path=str(archive.relative_to(ROOT)),archive_members_sha256=after,old_archive_sha256=sha(old),changed_members=changed)
 finally:
  path=out/'result.json';assert not path.exists();path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 text=(ROOT/'build/matrixgame/e2e600-campaign-navigation/fixture/sdk-snapshot-closure.py').read_text(encoding='utf-8')
 anchor="BASE=ROOT/'build/matrixgame/e2e600-campaign-navigation/sdk'";assert text.count(anchor)==1;text=text.replace(anchor,"BASE=ROOT/'build/matrixgame/e2e601-revolution-seed/sdk'")
 text=text.replace(repr(str(old)),repr(str(archive))).replace(repr(str(prior_path)),repr(str(out/'result.json')))
 fixture=base/'fixture';fixture.mkdir(exist_ok=True);script=fixture/'sdk-closure.py';assert not script.exists();script.write_text(text,encoding='utf-8');subprocess.run([sys.executable,script],check=True)

if __name__=='__main__':main()

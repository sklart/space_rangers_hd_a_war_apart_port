"""Rebuild performance logging in an isolated archive with configurable default."""
import hashlib,json,shutil,subprocess
from pathlib import Path
from matrixgame_legacy_wav_sdk_dependency import members
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e516-cfg-copy-sdk-20261124-1'
OUT=ROOT/'build/matrixgame/e2e516-detail-perf-sdk-20261124-1'
AR=Path('C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe')
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 if OUT.exists():raise RuntimeError('Output exists; preserve prior evidence')
 prior=json.loads((BASE/'result.json').read_text(encoding='utf-8'));old=Path(prior['archive'])
 assert prior['status'].startswith('PASS') and sha(old)==prior['archive_sha256']
 flags=json.loads((ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf-8'))
 source=ROOT/'port/switch/platform/e2e515_perf.cpp';header=ROOT/'port/switch/platform/e2e515_perf.hpp'
 OUT.mkdir(parents=True);result={'status':'INCOMPLETE','commands':[]}
 def run(label,command):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
  (OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
  print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 try:
  run('compile',[flags['compiler'],*flags['flags'],'-c',source,'-o',OUT/'e2e515_perf.o'])
  archive=OUT/'liboriginal-rangers-host-detail-perf.a';shutil.copyfile(old,archive);before=members(archive)
  run('replace',[AR,'rs',archive,OUT/'e2e515_perf.o']);after=members(archive)
  changed=sorted(k for k in before if before[k]!=after[k]);assert set(before)==set(after) and changed==['e2e515_perf.o']
  assert sha(old)==prior['archive_sha256']
  result.update(status='PASS SDK detailed performance archive; hardware NOT RUN',archive=str(archive),archive_sha256=sha(archive),
   changed_members=changed,source_sha256=sha(source),header_sha256=sha(header),archive_members_sha256=after)
 finally:(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

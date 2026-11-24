"""Recompile the owned E2E516 GR_Main object with safe CFG byte copying."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from matrixgame_legacy_wav_sdk_dependency import members

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e516-native-window-sdk-20261124-1'
OUT=ROOT/'build/matrixgame/e2e516-cfg-copy-sdk-20261124-1'
AR=Path('C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe')
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 if OUT.exists():raise RuntimeError('Output exists; preserve prior evidence')
 prior=json.loads((BASE/'result.json').read_text(encoding='utf-8'));old=Path(prior['archive']);src=BASE/'GR_Main.cpp'
 assert prior['status'].startswith('PASS') and sha(old)==prior['archive_sha256'] and sha(src)==prior['source_sha256']
 OUT.mkdir(parents=True);result={'status':'INCOMPLETE','commands':[]}
 def run(label,command):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
  (OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
  print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 try:
  command=next(c['command'] for c in prior['commands'] if c['label']=='compile-grmain')[:]
  command[command.index('-o')+1]=str(OUT/'GR_Main.o');run('compile-grmain',command)
  archive=OUT/'liboriginal-rangers-host-cfg-copy.a';shutil.copyfile(old,archive);before=members(archive)
  run('replace',[AR,'rs',archive,OUT/'GR_Main.o']);after=members(archive)
  changed=sorted(k for k in before if before[k]!=after[k]);assert set(before)==set(after) and changed==['GR_Main.o']
  assert sha(old)==prior['archive_sha256']
  result.update(status='PASS SDK CFG byte copy archive; hardware NOT RUN',archive=str(archive),archive_sha256=sha(archive),changed_members=changed,
                grmain_sha256=sha(src),cfg_header_sha256=sha(ROOT/'port/switch/platform/e2e5_cfg_bootstrap.hpp'),archive_members_sha256=after)
 finally:(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

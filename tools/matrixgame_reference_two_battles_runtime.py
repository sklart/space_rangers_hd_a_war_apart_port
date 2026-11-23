"""Run two natural Gov/FRun battles using one Windows process and campaign."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 out=BASE/'gov-frun-two-runtime-1'
 if out.exists():raise RuntimeError('runtime output already exists')
 out.mkdir();result={'status':'INCOMPLETE','scope':'two original Windows D3D9 Gov/FRun battles in one campaign; no Switch or UI autosave handler'}
 try:
  fixture=BASE/'gov-frun-runtime-1/native-input';output=out/'output';output.mkdir()
  files=[fixture/'CFG/robots/data.txt',fixture/'CFG/robots/iface.txt',fixture/'DATA/robots.pkg']
  assert all(p.is_file() for p in files);before={str(p):sha(p) for p in files}
  binary=BASE/'gov-frun-two-link/gov-frun-two.exe';assert binary.is_file()
  env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='windows')
  env['PATH']=str(BASE/'deps/sdl2/SDL2-2.30.11/lib/x64')+';'+str(BASE/'deps/d3dx/build/native/release/bin/x64')+';'+env.get('PATH','')
  command=[str(binary),str(output),str(ROOT/'windows/Space Rangers HD A War Apart'),str(fixture)]
  result.update(command=command,binary_sha256=sha(binary),fixture_sha256=before)
  print('two-battle runtime started; see runtime.log',flush=True)
  with (out/'runtime.log').open('w',encoding='utf8') as log:
   process=subprocess.run(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
  result['exit']=process.returncode;print('runtime exit',process.returncode,flush=True)
  if process.returncode:raise RuntimeError('reference process failed; see runtime.log')
  log=(out/'runtime.log').read_text(encoding='utf8',errors='replace')
  for token in ('GOV_FRUN battle_index=1 completed','GOV_FRUN battle_index=2 completed','original_campaign_UI_script_host_cleanup=PASS'):
   assert token in log,token
  assert log.count('original_Gov_history=PASS actual_statistics=PASS')==2
  assert log.count('CALLER_LEASE natural_return_window_owned=PASS')==2
  assert all(sha(Path(path))==digest for path,digest in before.items())
  result['status']='PASS; two natural Windows D3D9 Gov/FRun battles in one process; Switch NOT RUN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

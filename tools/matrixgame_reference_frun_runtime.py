"""Run the preserved natural Gov/FRun Windows reference with isolated outputs."""
import hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 out=BASE/'gov-frun-runtime-1'
 if out.exists():raise RuntimeError('runtime output already exists')
 out.mkdir();result={'status':'INCOMPLETE','scope':'one original Windows D3D9 Gov/FRun natural battle; no Switch; no UI autosave handler'}
 try:
  fixture=out/'native-input';data=out/'output';data.mkdir();fixture.mkdir()
  inputs={}
  for relative,source in [('CFG/robots/data.txt',ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/data.txt'),
                          ('CFG/robots/iface.txt',ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/iface.txt'),
                          ('DATA/robots.pkg',ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')]:
   target=fixture/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
   digest=sha(source);assert sha(target)==digest;inputs[str(source.relative_to(ROOT))]=digest
  binary=BASE/'gov-frun-link-3/gov-frun.exe';assert binary.is_file()
  env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='windows')
  env['PATH']=str(BASE/'deps/sdl2/SDL2-2.30.11/lib/x64')+';'+str(BASE/'deps/d3dx/build/native/release/bin/x64')+';'+env.get('PATH','')
  command=[str(binary),str(data),str(ROOT/'windows/Space Rangers HD A War Apart'),str(fixture)]
  result.update(command=command,binary_sha256=sha(binary),resource_inputs_sha256=inputs)
  print('runtime started; see runtime.log',flush=True)
  with (out/'runtime.log').open('w',encoding='utf8') as log:
   process=subprocess.run(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
  result['exit']=process.returncode;print('runtime exit',process.returncode,flush=True)
  if process.returncode:raise RuntimeError('reference process failed; see runtime.log')
  log=(out/'runtime.log').read_text(encoding='utf8',errors='replace')
  assert 'original_Gov_history=PASS actual_statistics=PASS' in log and 'original_campaign_UI_script_host_cleanup=PASS' in log
  assert all(sha(ROOT/path)==digest for path,digest in inputs.items())
  result['status']='PASS; one original Windows D3D9 natural Gov/FRun battle; second battle NOT RUN; Switch NOT RUN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

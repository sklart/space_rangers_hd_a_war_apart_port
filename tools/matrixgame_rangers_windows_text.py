"""MSVC AFT raster differential, reusing the completed hosted object corpus."""
import json
import subprocess
import time
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows'
def main():
 core=read(BASE/'result.json');assert core['status'].startswith('PASS')
 check_hashes(core['objects_sha256']);check_hashes(core['source_sha256'])
 out=BASE/'text'/('attempt-'+str(time.time_ns()));out.mkdir(parents=True)
 env=environment(out);env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
 source=ROOT/'tests/probe_matrixgame_rangers_text.cpp'
 result={'status':'INCOMPLETE','commands':[],'source_sha256':{str(source.relative_to(ROOT)):sha(source)},'reused_objects_sha256':core['objects_sha256']}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log,flush=True);assert not p.returncode;return log
 try:
  obj=out/'text.obj';binary=out/'rangers-text.exe'
  run([compiler(env),*core['flags'],'/c',source,'/Fo'+str(obj)],'compile-text')
  # Keep every original link input except the previous entry point.
  lines=(BASE/'link.rsp').read_text(encoding='utf-8').splitlines()
  entries=[line for line in lines if not line.strip('"').endswith('objects\\probe.obj')]
  assert len(entries)==len(lines)-1
  response=out/'link.rsp';response.write_text('\n'.join(entries+['"'+str(obj)+'"']),encoding='utf-8')
  link=next(item['command'] for item in core['commands'] if item['label']=='link')
  link=[('@'+str(response)) if arg.startswith('@') else ('/Fe'+str(binary)) if arg.startswith('/Fe') else arg for arg in link]
  run(link,'link-text');runtime=out/'runtime';runtime.mkdir()
  result['stdout']=run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],'runtime-text')
  assert 'exact_original_pairs=24 crop=1 automatic=1 error_cleanup=PASS' in result['stdout']
  pixels=runtime/'pixels.bin';reference=ROOT/'build/matrixgame/e2e570-rangers-text/host/runtime/pixels.bin'
  assert pixels.read_bytes()==reference.read_bytes() and pixels.stat().st_size==24*200*64*4
  result['pixels_sha256']=sha(pixels);result['reference_pixels_sha256']=sha(reference);result['binary_sha256']=sha(binary)
  result['status']='PASS; REAL WINDOWS AFT 24 ORIGINAL PAIRS CROP AUTO CLEANUP; EXACT LINUX PIXELS'
 finally:
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('result',out/'result.json',flush=True)
if __name__=='__main__':main()

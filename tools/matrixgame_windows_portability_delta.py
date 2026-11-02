"""Compile only changed portable units/consumers; execute the tiny clock probe.

This is not a rerun of the complete Rangers suites or an ARM/SDK runtime claim.
"""
import concurrent.futures
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows/delta'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'scope':'limited compile delta + host ASan/UBSan clock runtime; no full suite, ARM runtime, SDK link or NRO rebuild'}
 sources=[ROOT/'port/switch/platform'/name for name in ('e2e515_perf.cpp','win32_compat.cpp','win32_compat_files.cpp')]
 sources.append(ROOT/'port/switch/build/e2e-game-perf515/generated/aGalaxy.cpp')
 result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+[ROOT/'port/switch/platform/e2e515_save_checkpoint.hpp',ROOT/'port/switch/platform/e2e5_save_inventory.hpp']}
 host=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/asan/result.json')['commands'][0]['command']
 host_flags=host[3:host.index('-c')]
 arm=read(ROOT/'build/matrixgame/e2e576-rangers-arm64/result.json')['flags']
 sdk=read(ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final/result.json')['commands'][0]['command'];sdk_flags=sdk[1:sdk.index('-c')]
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  logpath=BASE/(label+'.log');i=1
  while logpath.exists():logpath=BASE/f'{label}-{i}.log';i+=1
  logpath.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(logpath.relative_to(ROOT))});print(label,p.returncode,log[-4000:] if p.returncode or label=='host-runtime' else '',flush=True);assert not p.returncode;return log
 try:
  jobs=[];objects={}
  for profile,compiler,flags in [('host-asan',['wsl','--exec','g++'],host_flags),('arm64',['wsl','--exec','aarch64-linux-gnu-g++'],arm),('sdk',[sdk[0]],sdk_flags)]:
   out=BASE/profile;out.mkdir(exist_ok=True)
   for source in sources:
    obj=out/(source.stem+'.o');objects[str(obj.relative_to(ROOT))]=obj
    jobs.append((compiler+flags+['-c',str(source) if profile=='sdk' else linux(source),'-o',str(obj) if profile=='sdk' else linux(obj)],profile+'-'+source.stem))
  with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:list(pool.map(lambda job:run(*job),jobs))
  probe=ROOT/'tests/probe_matrixgame_perf_clock.cpp';out=BASE/'host-asan';binary=out/'perf-clock'
  run(['wsl','--exec','g++',*host_flags,linux(probe),linux(out/'e2e515_perf.o'),'-pthread','-o',linux(binary)],'host-link')
  runtime=out/'runtime';i=1
  while runtime.exists():runtime=out/f'runtime-{i}';i+=1
  result['stdout']=run(['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',linux(binary),linux(runtime)],'host-runtime')
  assert 'writer_join=PASS' in result['stdout'];result['objects_sha256']={key:sha(p) for key,p in objects.items()}
  result['status']='PASS; TWELVE DELTA COMPILES HOST-ASAN ARM64 SDK + HOST SANITIZED CLOCK EXECUTION'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());target=BASE/'result.json';i=1
  while target.exists():target=BASE/f'result-{i}.json';i+=1
  target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(target,flush=True)
if __name__=='__main__':main()

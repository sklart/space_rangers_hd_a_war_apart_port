"""Execute actual Windows CPU timing and inspect epoch anchors in private logs."""
import json
import subprocess
import time
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows/perf-clock'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE)
 attempt=BASE/('attempt-'+str(time.time_ns()));attempt.mkdir()
 sources=[ROOT/'port/switch/platform/e2e515_perf.cpp',ROOT/'tests/probe_matrixgame_perf_clock.cpp']
 flags=['/nologo','/std:c++20','/EHsc','/utf-8','/MT','/O2','/UNDEBUG','/DNOMINMAX','/DE2E515_PERF_DIAGNOSTICS=1','/I'+str(ROOT/'port/switch/platform')]
 result={'status':'INCOMPLETE','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sources},'commands':[]}
 def run(command,label):
  p=subprocess.run(list(map(str,command)),cwd=attempt,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (attempt/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'exit':p.returncode});print(label,p.returncode,log,flush=True);assert not p.returncode;return log
 try:
  binary=attempt/'perf-clock.exe'
  run([compiler(env),*flags,*sources,'/Fe'+str(binary),'/link','/INCREMENTAL:NO'],'compile-link')
  start=int(time.time()*1000);result['stdout']=run([binary,attempt/'output'],'runtime');end=int(time.time()*1000)
  anchors=[]
  for path in (attempt/'output').rglob('*'):
   if path.is_file():
    for line in path.read_text(encoding='utf-8').splitlines():
     if line.startswith('anchor,'):anchors.append(line.split(','))
  assert anchors and all(start<=int(row[2])<=end and row[6]=='0' for row in anchors)
  result['anchors']=anchors;result['window_epoch_ms']=[start,end];result['binary_sha256']=sha(binary)
  result['status']='PASS; REAL WINDOWS THREAD CPU + WALL EPOCH ANCHOR + WRITER JOIN'
 finally:
  (attempt/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
  print('result',attempt/'result.json',flush=True)
if __name__=='__main__':main()

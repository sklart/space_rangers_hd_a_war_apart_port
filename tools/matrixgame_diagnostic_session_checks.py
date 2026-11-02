"""Actual portable session policy, Windows/host ASan+UBSan/ARM64 QEMU."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e586-interactive-diagnostic'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','host','arm64'));kind=parser.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);env=environment(out) if kind=='windows' else None;result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 source=ROOT/'tests/test_matrixgame_diagnostic_session.cpp';include=ROOT/'port/matrixgame/native-engine'
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=out/(label+'.log');i=1
  while path.exists():path=out/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode;return log
 try:
  if kind=='windows':
   binary=out/'session.exe';run([compiler(env),'/nologo','/std:c++20','/EHsc','/MT','/O2','/UNDEBUG','/utf-8','/I'+str(include),source,'/Fo'+str(out/'test.obj'),'/Fe'+str(binary)],'build');runtime=[binary]
  else:
   binary=out/'session';cxx='/usr/bin/c++' if kind=='host' else '/usr/bin/aarch64-linux-gnu-g++-15';flags=['-std=c++20','-O1','-UNDEBUG','-fno-omit-frame-pointer','-no-pie']
   if kind=='host':flags+=['-fsanitize=address,undefined']
   run(['wsl','--exec',cxx,*flags,'-I'+linux(include),linux(source),'-o',linux(binary)],'build')
   runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   runtime+=[linux(binary)]
  result['stdout']=run(runtime,'runtime');assert 'modes=2 button_cases=16 quick_four_frames=PASS manual_no_autostop=PASS' in result['stdout'] and 'phase_deadlines=PASS counters_throttled=PASS' in result['stdout']
  result['binary_sha256']=sha(binary);result['status']='PASS; '+kind.upper()+' SESSION CONTROL POLICY; CPU NO GPU'
 finally:
  files=[source,include/'native_diagnostic_session.hpp',include/'native_diagnostic_control.hpp',Path(__file__).resolve()];result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in files}
  path=out/'result.json';i=1
  while path.exists():path=out/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

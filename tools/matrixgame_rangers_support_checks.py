"""Execute original Support code ordering/boundaries as a CPU policy."""
import argparse,json,subprocess
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e592-native-rangers-interface/policy'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','host','arm64'));kind=parser.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);env=environment(out) if kind=='windows' else None
 source=ROOT/'tests/test_matrixgame_rangers_support.cpp';include=ROOT/'port/matrixgame/native-engine';binary=out/('support.exe' if kind=='windows' else 'support');r={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=out/(label+'.log');index=1
  while path.exists():path=out/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');r['commands'].append({'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(kind,label,p.returncode,log[-2500:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode;return log
 try:
  if kind=='windows':build=[compiler(env),'/nologo','/std:c++20','/EHsc','/MT','/O2','/UNDEBUG','/I'+str(include),source,'/Fo'+str(out/'support.obj'),'/Fe'+str(binary)];runtime=[binary]
  else:
   cxx='/usr/bin/c++' if kind=='host' else '/usr/bin/aarch64-linux-gnu-g++-15';flags=['-std=c++20','-O1','-UNDEBUG','-no-pie','-fno-omit-frame-pointer']
   if kind=='host':flags+=['-fsanitize=address,undefined']
   build=['wsl','--exec',cxx,*flags,'-I'+linux(include),linux(source),'-o',linux(binary)]
   runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   runtime+=[linux(binary)]
  run(build,'build');r['stdout']=run(runtime,'runtime');assert 'original_code_order=PASS texture_width_boundary=PASS stream_boundary=PASS query_failure=PASS CPU_POLICY_ONLY' in r['stdout']
  r.update(status='PASS; '+kind.upper()+' RANGERS SUPPORT POLICY EXECUTED; CPU NO GPU',source_sha256={str(p.relative_to(ROOT)):sha(p) for p in (source,include/'native_rangers_support.hpp')},binary_sha256=sha(binary))
 finally:
  path=out/'result.json';index=1
  while path.exists():path=out/f'result-{index}.json';index+=1
  path.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

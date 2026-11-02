"""Real rejected game WAV and unchanged strict corpus on Windows/ASan/ARM64."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e590-completed-battle/wav'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','host','arm64'));kind=parser.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);env=environment(out) if kind=='windows' else None
 wave=ROOT/'build/matrixgame/e2e590-completed-battle/fixed/runtime/output/failed-wave.bin';assert sha(wave)=='56baed668848829f3f971c697946a2a90532fab057608630f961e1bc4e93d5a8'
 sources=[ROOT/'tests/test_matrixgame_legacy_wav.cpp',ROOT/'tests/test_e2e4_wav.cpp'];include=ROOT/'port/switch/platform'
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=out/(label+'.log');index=1
  while path.exists():path=out/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(kind,label,p.returncode,log[-3000:] if p.returncode or label.startswith('runtime') else '',flush=True);assert not p.returncode;return log
 try:
  binaries=[]
  for i,source in enumerate(sources):
   binary=out/(source.stem+('.exe' if kind=='windows' else ''));binaries.append(binary)
   if kind=='windows':
    command=[compiler(env),'/nologo','/std:c++20','/EHsc','/MT','/O2','/UNDEBUG','/I'+str(include),source,'/Fo'+str(out/(source.stem+'.obj')),'/Fe'+str(binary)];runtime=[binary,*([wave] if i==0 else [])]
   else:
    cxx='/usr/bin/c++' if kind=='host' else '/usr/bin/aarch64-linux-gnu-g++-15';flags=['-std=c++20','-O1','-UNDEBUG','-no-pie','-fno-omit-frame-pointer']
    if kind=='host':flags+=['-fsanitize=address,undefined']
    command=['wsl','--exec',cxx,*flags,'-I'+linux(include),linux(source),'-o',linux(binary)]
    runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
    if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
    runtime+=[linux(binary),*([linux(wave)] if i==0 else [])]
   run(command,'build-'+str(i));stdout=run(runtime,'runtime-'+str(i))
   if i==0:assert 'original_Sound_Flame=PASS strict_rejection_unchanged=PASS' in stdout;result['stdout']=stdout
  result['binary_sha256']={str(p.relative_to(ROOT)):sha(p) for p in binaries};result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in [*sources,include/'e2e4_wav.hpp',wave,Path(__file__).resolve()]};result['status']='PASS; '+kind.upper()+' ORIGINAL GAME WAV COMPAT AND STRICT MALFORMED CORPUS EXECUTED; NO GPU'
 finally:
  path=out/'result.json';index=1
  while path.exists():path=out/f'result-{index}.json';index+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

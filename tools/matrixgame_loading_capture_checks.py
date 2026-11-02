"""Owned loading snapshot oracle; synthetic pixels, no GPU/runtime claim."""
import argparse
import json
import shlex
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e580-loading-capture'
ENGINE=BASE/'native-build/native/source'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','host','arm64'));parser.add_argument('--gpu',action='store_true');args=parser.parse_args();kind=args.kind
 assert not args.gpu or kind=='windows'
 out=BASE/('capture-'+kind+('-gpu' if args.gpu else ''));out.mkdir(parents=True,exist_ok=True)
 env=environment(out) if kind=='windows' else None
 sources=[ROOT/'tests/test_matrixgame_loading_capture.cpp',ROOT/'port/matrixgame/native-engine/transition_backend.cpp',ROOT/'port/matrixgame/native-engine/native_screenshot.cpp',ROOT/'port/matrixgame/native-engine/platform_3g.cpp']
 if args.gpu:sources[0]=ROOT/'tests/test_matrixgame_loading_capture_windows_gpu.cpp'
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sources}}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
  log=p.stdout+p.stderr;path=out/(label+'.log');i=1
  while path.exists():path=out/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))})
  print(label,p.returncode,log[-7000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0;return log
 try:
  if kind=='windows':
   commands=read(BASE/'native-build/native/compile_commands.json');template=next(c for c in commands if c['file'].endswith('init_runtime.cpp'))
   flags=shlex.split(template['command'],posix=False)[1:];flags=[s for s in flags if s not in ('-c',template['file'],'/FS') and not s.startswith(('/Fo','/Fd'))]+['/UNDEBUG','/Gy','/Gw']
   objects=[]
   for source in sources:
    obj=out/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
   native=read(BASE/'native-build/build-result.json');inputs=[*objects,*[ROOT/s for s in {**native['archives_sha256'],**native['image_inputs_sha256']}]]
   response=out/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=out/'capture.exe'
   run([compiler(env),'/nologo','/MT','@'+str(response),'/Fe'+str(binary),'/link','/OPT:REF','/INCREMENTAL:NO','d3d9.lib','user32.lib','gdi32.lib','winmm.lib','shell32.lib','ole32.lib'],'link');runtime=[binary]
  else:
   prior=read(ROOT/f'build/matrixgame/e2e568-native-services/{kind}/result.json');template=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime');flags=[];i=3
   while i<len(template):
    if template[i] in ('-c','-o'):i+=2;continue
    flags.append(template[i].replace(linux(ROOT/'build/matrixgame/e2e567-screenshot/sdk/source'),linux(ENGINE)));i+=1
   objects=[]
   for source in sources:
    obj=out/(source.stem+'.o');run([*template[:3],*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(linux(obj))
   prior_link=next(c['command'] for c in prior['commands'] if c['label']=='link')
   libraries=[s for s in prior_link[3:] if s.endswith(('.a','.so')) or s in ('-lm','-fsanitize=address,undefined','-fno-omit-frame-pointer')]
   # Only CBitmap/Base is needed; omit unrelated old engine archive objects.
   libraries=[s for s in libraries if not s.endswith('.a') or 'native-core.a' in s or 'native-cache.a' not in s and 'native-vector-object.a' not in s and 'native-navigation.a' not in s]
   binary=out/'capture';run([*template[:3],'-Wl,--gc-sections','-no-pie',*objects,*libraries,'-o',linux(binary)],'link')
   runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   runtime+=[linux(binary)]
   result['archives_sha256']={s:sha(Path(s.replace('/mnt/d/','D:/'))) for s in libraries if s.endswith('.a')}
  result['stdout']=run(runtime,'runtime')
  if args.gpu:assert 'actual_completed_readback=PASS reset_repaint_owned_pixels=PASS rgb_oracle=18432 two_cycles=PASS one_shot=PASS COM_cleanup=PASS' in result['stdout']
  else:assert 'formats_cycles=4 owned_snapshot=PASS prior_slot_reset=PASS' in result['stdout'] and 'failures_retry=5 cleanup=PASS' in result['stdout']
  result['binary_sha256']=sha(binary);result['status']='PASS; OWNED LOADING SNAPSHOT; '+('ACTUAL WINDOWS HAL RESET READBACK; NO SWITCH' if args.gpu else 'SYNTHETIC CPU PIXELS NO GPU; '+kind.upper())
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());path=out/'result.json';i=1
  while path.exists():path=out/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(path,flush=True)
if __name__=='__main__':main()

"""Actual texture dynamic types, CPU material compatibility and scope cleanup."""
import argparse
import json
import shlex
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map'
ENGINE=BASE/'native-build/native/source'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','host','arm64'));kind=parser.parse_args().kind
 out=BASE/('factory-'+kind);out.mkdir(exist_ok=True);env=environment(out) if kind=='windows' else None
 sources=[ROOT/'tests/test_matrixgame_original_texture_factory.cpp',ROOT/'port/matrixgame/native-engine/cache_resources.cpp',ROOT/'port/matrixgame/native-engine/original_game.cpp']
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sources}}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  path=out/(label+'.log');i=1
  while path.exists():path=out/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-9000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode;return log
 try:
  if kind=='windows':
   compile_commands=read(BASE/'native-build/native/compile_commands.json');template=next(e for e in compile_commands if e['file'].endswith('init_runtime.cpp'))
   flags=shlex.split(template['command'],posix=False)[1:];flags=[s for s in flags if s not in ('-c',template['file'],'/FS') and not s.startswith(('/Fo','/Fd'))]+['/UNDEBUG']
   obj=out/'test.obj';run([compiler(env),*flags,'/c',sources[0],'/Fo'+str(obj)],'compile-test')
   records=sorted((BASE/'native-build').glob('build-result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);native=next(read(p) for p in records if read(p)['status'].startswith('PASS'))
   libraries=[ROOT/name for name in {**native['archives_sha256'],**native['image_inputs_sha256']}]
   d3dx=ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8/build/native/release/lib/x64/d3dx9.lib'
   inputs=[obj,*libraries,d3dx];response=out/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=out/'factory.exe'
   run([compiler(env),'/nologo','/MT','@'+str(response),'/Fe'+str(binary),'/link','/OPT:REF','/INCREMENTAL:NO','d3d9.lib','user32.lib','gdi32.lib','winmm.lib','shell32.lib','ole32.lib','oleaut32.lib'],'link')
   runtime=[binary];result['link_inputs_sha256']={str(p):sha(p) for p in inputs}
  else:
   prior=read(ROOT/f'build/matrixgame/e2e568-native-services/{kind}/result.json')
   template=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime')
   flags=template[3:];removed=[];i=0
   while i<len(flags):
    if flags[i] in ('-c','-o'):i+=2;continue
    removed.append(flags[i]);i+=1
   flags=[s.replace(linux(ROOT/'build/matrixgame/e2e567-screenshot/sdk/source'),linux(ENGINE)) for s in removed]
   objects=[]
   for source in sources:
    obj=out/(source.stem+'.o');run([*template[:3],*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(linux(obj))
   command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:]
   command=[s for s in command if not s.endswith(('/original_game.o','/test_matrixgame_native_services.o'))]
   command[command.index('-o'):command.index('-o')]=objects;binary=out/'factory';command[command.index('-o')+1]=linux(binary)
   run(command,'link');runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1']
   if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   runtime+=[linux(binary)];result['archives_sha256']={s:sha(Path(s.replace('/mnt/d/','D:/'))) for s in command if s.endswith('.a')}
  result['stdout']=run(runtime,'runtime');assert 'actual_dynamic_types=PASS cpu_material_preserved=PASS exclusive_scope=PASS cleanup_reentry=PASS' in result['stdout']
  result['binary_sha256']=sha(binary);result['status']='PASS; ACTUAL ORIGINAL TEXTURE TYPES AND CPU MATERIAL CONTRACT; '+kind.upper()
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());path=out/'result.json';i=1
  while path.exists():path=out/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(path,flush=True)
if __name__=='__main__':main()

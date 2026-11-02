"""Actual Windows native CGame::Init with real Rangers owners, early caps refusal.

The deliberate unsupported device is the same CPU-only boundary used on host
and ARM64. No GPU operation is reported successful by this fixture.
"""
import argparse
import json
import shlex
import shutil
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e578-hosted-windows-native'
HOST=ROOT/'build/matrixgame/e2e577-rangers-windows'
SOURCE=ROOT/'tests/probe_matrixgame_hosted_native_prefix.cpp'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--compile-only',action='store_true');args=parser.parse_args()
 out=BASE/'prefix';out.mkdir(parents=True,exist_ok=True);env=environment(out)
 env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
 host=read(HOST/'result.json');assert host['status'].startswith('PASS');check_hashes(host['objects_sha256'])
 commands=read(BASE/'native/compile_commands.json');template=next(item for item in commands if item['file'].endswith('init_runtime.cpp'))
 flags=shlex.split(template['command'],posix=False)[1:]
 flags=[s for s in flags if s not in ('-c',template['file'],'/FS') and not s.startswith(('/Fo','/Fd'))]
 flags += host['flags']+['/UNDEBUG']
 source=SOURCE;obj=out/'prefix.obj'
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','new_NRO':False,'source_sha256':{str(source.relative_to(ROOT)):sha(source)},'flags':flags}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  path=out/(label+'.log');i=1
  while path.exists():path=out/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-14000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode;return log
 try:
  run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-prefix')
  result['object_sha256']=sha(obj)
  if args.compile_only:result['status']='PASS; PREFIX COMPILE ONLY';return
  builds=sorted(BASE.glob('build-result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);native=next(read(p) for p in builds if read(p)['status'].startswith('PASS'))
  check_hashes(native['archives_sha256']);check_hashes(native['image_inputs_sha256'])
  # The shared real package TU is already supplied by native-core.
  reused={name:digest for name,digest in host['objects_sha256'].items() if not name.endswith(('\\probe.obj','\\package.obj'))}
  assert len(reused)==337
  old_entries=[line.strip('"') for line in (HOST/'link.rsp').read_text().splitlines()]
  libraries=[Path(name) for name in old_entries if name.endswith('.lib')]
  d3dx=ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8/build/native/release/lib/x64/d3dx9.lib'
  archives=[ROOT/name for name in native['archives_sha256']]
  inputs=[obj,*map(Path,reused),*archives,*libraries,d3dx]
  result['link_inputs_sha256']={str(p):sha(p) for p in inputs}
  response=out/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8')
  binary=out/'hosted-native-prefix.exe'
  run([compiler(env),'/nologo','/MT','@'+str(response),'/Fe'+str(binary),'/link','/OPT:REF','/INCREMENTAL:NO','d3d9.lib','oleaut32.lib','ole32.lib','user32.lib','gdi32.lib','version.lib','winmm.lib','setupapi.lib','imm32.lib','advapi32.lib','shell32.lib','uuid.lib'],'link')
  fixture=out/'native-input';fixture.mkdir(exist_ok=True)
  fixture_inputs={}
  for relative,source_file in [('CFG/robots/data.txt',ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/data.txt'),('CFG/robots/iface.txt',ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/iface.txt'),('DATA/robots.pkg',ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')]:
   destination=fixture/relative;destination.parent.mkdir(parents=True,exist_ok=True)
   if not destination.exists():shutil.copyfile(source_file,destination)
   assert sha(source_file)==sha(destination);fixture_inputs[str(source_file)]=sha(source_file)
  runtime=out/'runtime';runtime.mkdir(exist_ok=True)
  result['stdout']=run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime,fixture],'runtime')
  assert 'original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS' in result['stdout']
  assert result['stdout'].count('original Init exception: HOSTED_NATIVE_CAPS_BOUNDARY')==2
  check_hashes(fixture_inputs);check_hashes(reused);result['reused_host_objects_sha256']=reused
  result['fixture_inputs_sha256']=fixture_inputs;result['binary_sha256']=sha(binary)
  result['status']='PASS; WINDOWS REAL HOSTED ORIGINAL INIT TWO CAPS-REFUSAL ROLLBACK CYCLES; NO GPU/FULL INIT'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());path=out/'result.json';i=1
  while path.exists():path=out/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('result',path,flush=True)
if __name__=='__main__':main()

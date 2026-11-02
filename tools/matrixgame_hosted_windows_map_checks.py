"""First actual PC D3D9 map execution with the real owning Rangers host.

Reuses current Windows native/Rangers objects; executes only this new joint
composition, without rerunning provider preflight/selection/input suites.
"""
import json
import hashlib
import shlex
import shutil
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map'
NATIVE=BASE/'native-build'
FIXTURE=ROOT/'build/matrixgame/e2e578-hosted-windows-native/prefix/native-input'
HOST=ROOT/'build/matrixgame/e2e577-rangers-windows'
PROBE=ROOT/'tests/probe_matrixgame_hosted_windows_map.cpp'
EXTRA_SOURCES=[]
EXTRA_FLAGS=[]
EXCLUDED_HOST_OBJECTS=[]
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
 core=read(HOST/'result.json');assert core['status'].startswith('PASS');check_hashes(core['objects_sha256'])
 builds=sorted(NATIVE.glob('build-result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True)
 native={'build':next(read(p) for p in builds if read(p)['status'].startswith('PASS'))};check_hashes(native['build']['archives_sha256'])
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','new_NRO':False}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=300);log=p.stdout+p.stderr
  path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))})
  display=''
  if label=='runtime':display='\n'.join(line for line in log.splitlines() if line.startswith(('HOSTED_MAP','D3D9 HAL','original Init entry','actual map framebuffer','actual gamma/window','actual D3D9 Reset')))
  elif p.returncode:display=log[-10000:]
  print(label,p.returncode,display,flush=True);assert not p.returncode;return log
 try:
  # Existing genuine SDK window/device/readback owner, without re-executing its
  # already-passed unrelated video-provider preflight. Its cleanup assertions stay.
  text=(ROOT/'tests/matrixgame_windows_frame_main.cpp').read_text()
  replacements={'if(argc!=2)return 2;':'if(argc!=4)return 2;','    CheckVideoProviders(window,device,module,raw);':'    ConfigureHostedRoots(argv[2],argv[3],argv[1]);'}
  for old,new in replacements.items():assert text.count(old)==1;text=text.replace(old,new,1)
  text='void ConfigureHostedRoots(const char*,const char*,const char*);\n'+text
  main=BASE/'hosted_main.cpp';main.write_text(text,encoding='utf-8')
  sources=[main,PROBE,NATIVE/'native/source/MatrixGame/src/OriginalWindowsFrame.cpp',*EXTRA_SOURCES]
  provenance={'sources':{str(p):sha(p) for p in sources},'native_archives':native['build']['archives_sha256'],'host_objects':core['objects_sha256']}
  result['composition_source_sha256']=hashlib.sha256(json.dumps(provenance,sort_keys=True).encode()).hexdigest()
  (BASE/'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="'+result['composition_source_sha256']+'";\n',encoding='utf-8')
  commands=read(NATIVE/'native/compile_commands.json');template=next(item for item in commands if item['file'].endswith('init_runtime.cpp'))
  flags=shlex.split(template['command'],posix=False)[1:]
  flags=[s for s in flags if s not in ('-c',template['file'],'/FS') and not s.startswith(('/Fo','/Fd'))]
  flags+=core['flags']+['/UNDEBUG','/I'+str(BASE),*EXTRA_FLAGS];result['flags']=flags
  objects=[]
  for source in sources:
   obj=BASE/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  resource=BASE/'frame.res';rc=shutil.which('rc.exe',path=env['PATH']);assert rc
  run([rc,'/nologo','/I',ROOT/'upstream/matrixgame/MatrixGame','/fo',resource,ROOT/'tests/matrixgame_windows_frame.rc'],'resource');objects.append(resource)
  reused={name:digest for name,digest in core['objects_sha256'].items() if not name.endswith(('\\probe.obj','\\package.obj'))};assert len(reused)==337
  for basename in EXCLUDED_HOST_OBJECTS:
   matches=[name for name in reused if Path(name).name==basename];assert len(matches)==1,basename
   del reused[matches[0]]
  libraries=[Path(line.strip('"')) for line in (HOST/'link.rsp').read_text().splitlines() if line.strip('"').endswith('.lib')]
  package=ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8/build/native'
  archives=[ROOT/name for name in native['build']['archives_sha256']]
  inputs=[*objects,*map(Path,reused),*archives,*libraries,package/'release/lib/x64/d3dx9.lib']
  response=BASE/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8')
  binary=BASE/'hosted-map.exe'
  run([compiler(env),'/nologo','/MT',*EXTRA_FLAGS,'@'+str(response),'/Fe'+str(binary),'/link','/OPT:REF','/INCREMENTAL:NO','d3d9.lib','oleaut32.lib','ole32.lib','user32.lib','gdi32.lib','version.lib','winmm.lib','setupapi.lib','imm32.lib','advapi32.lib','shell32.lib','uuid.lib'],'link')
  for name in ('D3DX9_43.dll','D3DCompiler_43.dll'):
   source=package/'release/bin/x64'/name;dest=BASE/name
   if not dest.exists():shutil.copyfile(source,dest)
   assert sha(source)==sha(dest)
  runtime=BASE/'runtime';runtime.mkdir(exist_ok=True);output=runtime/'output';output.mkdir(exist_ok=True)
  result['link_inputs_sha256']={str(p):sha(p) for p in inputs};result['sources_sha256']={str(p):sha(p) for p in sources}
  result['stdout']=run([binary,output,ROOT/'windows/Space Rangers HD A War Apart',FIXTURE],'runtime',runtime)
  assert result['stdout'].count('full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS')==2
  assert result['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and result['stdout'].count('readback_present=PASS')==8
  result['binary_sha256']=sha(binary);result['outputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in output.rglob('*') if p.is_file()}
  result['status']='PASS; REAL WINDOWS HAL HOSTED ORIGINAL FULL INIT MAP EIGHT FRAMES TWO CLEANUPS; NO SWITCH'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());path=BASE/'result.json';i=1
  while path.exists():path=BASE/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('result',path,flush=True)
if __name__=='__main__':main()

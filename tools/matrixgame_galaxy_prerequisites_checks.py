"""Execute real Hangar/TCC/Galaxy construction using verified E595 dependencies."""
import argparse,json,os,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e596-galaxy-prerequisites'
PREVIOUS=ROOT/'build/matrixgame/e2e595-script-session'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def latest(folder):
 r=read(max(folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));assert r['status'].startswith('PASS');return r
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows-asan','asan','arm64'));kind=parser.parse_args().kind
 parent=BASE/kind;parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();prior=latest(PREVIOUS/kind)
 result={'status':'INCOMPLETE','commands':[],'campaign_generation':'NOT RUN','Switch':'NOT RUN','attempt':i}
 sources=[ROOT/'port/matrixgame/native/rangers_galaxy_prerequisites.cpp',ROOT/'tests/probe_matrixgame_galaxy_prerequisites.cpp'];env=dict(os.environ)
 def run(cmd,label,cwd=ROOT,timeout=180):
  cmd=list(map(str,cmd))
  try:
   p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  path=out/(label+'.log');path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':code,'log':str(path.relative_to(ROOT))})
  print(kind,label,code,log[-7000:] if code or label=='runtime' else '',flush=True);assert code==0,label;return log
 try:
  flags=prior['flags'];objects=[]
  if kind=='windows-asan':
   from matrixgame_windows_host_env import environment,compiler
   env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy')
   check_hashes(prior['link_inputs_sha256']);old_response=PREVIOUS/kind/'link.rsp'
   reused=[Path(line.strip('"')) for line in old_response.read_text().splitlines() if Path(line.strip('"')).name!='probe_matrixgame_script_session.obj']
   for source in sources:
    obj=out/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
   inputs=reused+objects;response=out/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=out/'galaxy-prerequisites.exe'
   command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:]
   command=[('@'+str(response)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];run(command,'link')
   runtime=out/'runtime';runtime.mkdir();env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH']
   runtime_command=[binary,ROOT/'windows/Space Rangers HD A War Apart',runtime];result['link_inputs_sha256']={str(p):sha(p) for p in inputs}
  else:
   reused={k:v for k,v in {**prior['reused_objects_sha256'],**prior['objects_sha256']}.items() if not k.endswith('/probe_matrixgame_script_session.o') and not k.endswith('\\probe_matrixgame_script_session.o')};check_hashes(reused)
   cxx='g++' if kind=='asan' else '/usr/bin/aarch64-linux-gnu-g++'
   for source in sources:
    obj=out/(source.stem+'.o');run(['wsl','--exec',cxx,*flags,'-I'+linux(ROOT/'port/matrixgame/native'),'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(obj)
   binary=out/'galaxy-prerequisites';manifest={'objects':[linux(ROOT/k) for k in reused]+[linux(p) for p in objects],'output':linux(binary),'report':linux(out/'link-report.json'),'cxx':cxx,'link_flags':['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else [],'libraries':[linux(ROOT/k) for k in prior['libraries_sha256']],'okgf':linux(ROOT/'port/switch/build/e2e5-host-game/switch-effective/okgf/libokgf.so')}
   (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(out/'link-input.json')],'link')
   runtime=out/'runtime';runtime.mkdir();runtime_command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
   if kind=='asan':runtime_command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   else:runtime_command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   runtime_command += [linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)]
   check_hashes(reused);result.update(reused_objects_sha256=reused,libraries_sha256=prior['libraries_sha256'])
  result['stdout']=run(runtime_command,'runtime')
  assert result['stdout'].count('original_Hangar_config_layout=PASS original_TCCInterface=PASS original_TGalaxy_create_destroy=PASS integrity_operations=UNMODIFIED campaign_Player=NOT_GENERATED')==2
  assert 'partial_layout_rollback=PASS foreign_TCC_roots_duplicate=REJECTED original_cleanup_borrowed_roots=PASS no_save_load=PASS campaign_generation=NOT_RUN' in result['stdout']
  result.update(status='PASS; '+kind.upper()+' ORIGINAL HANGAR/TCC/GALAXY LIFECYCLE; NO GENERATED CAMPAIGN',flags=flags,new_TUs_instrumented=kind!='arm64',original_dependencies_instrumented=False,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_galaxy_prerequisites.hpp',Path(__file__).resolve()]},objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary=str(binary.relative_to(ROOT)),binary_sha256=sha(binary))
 finally:
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

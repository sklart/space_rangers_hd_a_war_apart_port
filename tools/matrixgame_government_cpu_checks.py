"""Real government tree/layout/rollback on host sanitizers and ARM64/QEMU; reuse original TUs."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e594-government-session'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('asan','arm64'));kind=parser.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','OnOpen_Galaxy':'NOT RUN'}
 sources=[ROOT/'port/matrixgame/native/rangers_government_session.cpp',ROOT/'tests/probe_matrixgame_government_layout.cpp']
 if kind=='asan':
  prior=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/asan/result.json');assert prior['status'].startswith('PASS')
  reused={k:v for k,v in {**prior['reused_objects_sha256'],**prior['new_objects_sha256']}.items() if not k.endswith('probe_matrixgame_rangers_host_session.o')}
  template=next(c['command'] for c in prior['commands'] if c['label']=='compile-rangers_host_session')
  # The command starts wsl --exec c++; retain all flags through -c.
  flags=template[3:template.index('-c')];cxx=template[2];link_flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'];libraries=[]
 else:
  prior=read(ROOT/'build/matrixgame/e2e576-rangers-arm64/result.json');assert prior['status'].startswith('PASS')
  reused={k:v for k,v in prior['objects_sha256'].items() if not k.endswith('/probe.o')}
  flags=prior['flags'];cxx='/usr/bin/aarch64-linux-gnu-g++';link_flags=[];libraries=[linux(ROOT/k) for k in prior['libraries_sha256']]
 check_hashes(reused)
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180);log=p.stdout+p.stderr;path=out/(label+'.log');i=1
  while path.exists():path=out/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(kind,label,p.returncode,log[-5500:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label;return log
 try:
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.o');run(['wsl','--exec',cxx,*flags,'-I'+linux(ROOT/'port/matrixgame/native'),'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(obj)
  manifest={'objects':[linux(ROOT/k) for k in reused]+[linux(p) for p in objects],'output':linux(out/'government'),'report':linux(out/'link-report.json'),'cxx':cxx,'link_flags':link_flags,'libraries':libraries,'okgf':linux(ROOT/'port/switch/build/e2e5-host-game/switch-effective/okgf/libokgf.so')}
  (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(out/'link-input.json')],'link')
  runtime=out/'runtime';runtime.mkdir(exist_ok=True);command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
  if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  else:command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  result['stdout']=run([*command,linux(out/'government'),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)],'runtime')
  assert result['stdout'].count('original_Gov_constructor_config_layout=PASS real_callbacks=PASS duplicate_owner=REJECTED OnOpen_Galaxy=NOT_RUN')==2
  assert 'original_partial_tree_failure_rollback=PASS' in result['stdout'] and 'borrowed_root_restore=PASS foreign_roots=REJECTED no_save_load=PASS CPU_LAYOUT_ONLY' in result['stdout']
  check_hashes(reused)
  result.update(status='PASS; '+kind.upper()+' ORIGINAL GOVERNMENT UI LAYOUT/ROLLBACK/CLEANUP EXECUTED; NO ONOPEN/GALAXY',new_TUs_instrumented=kind=='asan',original_dependencies_instrumented=False,source_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_government_session.hpp',Path(__file__).resolve(),ROOT/'tools/matrixgame_government_cpu_link.py']},reused_objects_sha256=reused,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},libraries_sha256=prior.get('libraries_sha256',{}),binary_sha256=sha(out/'government'),flags=flags)
 finally:
  path=out/'result.json';i=1
  while path.exists():path=out/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

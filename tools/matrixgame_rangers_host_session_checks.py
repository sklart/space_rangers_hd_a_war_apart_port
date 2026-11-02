"""Real standalone host ownership and all four original audio mode combinations."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
BASE=ROOT/'build/matrixgame/e2e574-rangers-host-session'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','asan'));kind=parser.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 prior=read(ROOT/f'build/matrixgame/e2e573-rangers-data/{kind}-final/result.json')
 reused={name:value for name,value in prior['reused_objects_sha256'].items() if not name.endswith('engine_callbacks.o')}
 reused.update({name:value for name,value in prior['new_objects_sha256'].items() if name.endswith(('rangers_music_session.o','rangers_data_session.o','EC_BlockPar.o','EC_Data.o'))});check_hashes(reused);assert len(reused)==335
 sources=[ROOT/'port/matrixgame/native'/name for name in ('engine_callbacks.cpp','rangers_audio_session.cpp','rangers_host_session.cpp')]+[ROOT/'tests/probe_matrixgame_rangers_host_session.cpp']
 headers=[ROOT/'port/matrixgame/native'/name for name in ('rangers_host_session.hpp','engine_callbacks.hpp','rangers_audio_session.hpp','rangers_data_session.hpp','rangers_music_session.hpp')]
 before={str(path.relative_to(ROOT)):sha(path) for path in sources+headers+[Path(__file__).resolve()]}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(kind,label,p.returncode,log[-6000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label;return log
 try:
  objects=[]
  template=next(c['command'] for c in prior['commands'] if c['label']=='compile-rangers_audio_session')
  for source in sources:
   command=template[:];obj=out/(source.stem+'.o');command[command.index('-c')+1]=linux(source);command[command.index('-o')+1]=linux(obj);run(command,'compile-'+source.stem);objects.append(obj)
  sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else []
  manifest={'objects':[linux(path) for path in objects]+[linux(ROOT/name) for name in reused],'output':linux(out/'rangers-host-session'),'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(out/'link-report.json'),'link_flags':sanitizer}
  (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(out/'link-input.json')],'link')
  command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
  if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  command += [linux(out/'rangers-host-session'),linux(GAME),linux(out/'runtime')]
  result['stdout']=run(command,'runtime');assert 'modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS' in result['stdout']
  check_hashes(before);check_hashes(reused);check_hashes(prior['protected_before_after'])
  result['reused_objects_sha256']=reused;result['new_objects_sha256']={str(path.relative_to(ROOT)):sha(path) for path in objects}
  result['protected_game_sha256']=prior['protected_before_after'];result['binary_sha256']=sha(out/'rangers-host-session');result['new_TUs_instrumented']=kind=='asan';result['original_dependencies_instrumented']=False
  result['status']='PASS; REAL HOST SESSION FOUR AUDIO MODES; SDL DUMMY ONLY'
 finally:
  result['source_sha256']=before;(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

"""Original full campaign on host sanitizers or ARM64/QEMU, reusing prior objects."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_campaign_overrides import apply
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e598-campaign'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('asan','arm64'));kind=parser.parse_args().kind
 old=ROOT/'build/matrixgame/e2e597-global-ui'/kind;prior=read(max(old.glob('*/result.json'),key=lambda p:p.stat().st_mtime_ns));assert prior['status'].startswith('PASS')
 reused={k:v for k,v in {**prior['reused_objects_sha256'],**prior['objects_sha256']}.items() if Path(k.replace('\\','/')).name not in ('fGameSettings.o','rangers_data_session.o','probe_matrixgame_global_ui.o')};check_hashes(reused);check_hashes(prior['libraries_sha256'])
 parent=BASE/kind;parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json');original=ROOT/mapping['upstream/cpp/src/fGameSettings.cpp']
 fixed=out/'fGameSettings.cpp';fixed.write_text(apply(original.read_text(encoding='utf-8')),encoding='utf-8')
 prefix=(ROOT/'tests/probe_matrixgame_global_ui.cpp').read_text(encoding='utf-8');probe=out/'campaign_probe.cpp'
 probe.write_text('#include "rangers_campaign_session.hpp"\n#include "e2e3_generation_state.hpp"\n#include "units/aRanger.hpp"\n#include "types/aMyFunction.hpp"\n'+prefix[:prefix.index('int main(int argc,char** argv){')]+(ROOT/'tests/matrixgame_campaign_probe.inc').read_text(encoding='utf-8'),encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','battle_Gov_return':'NOT RUN','attempt':i,'original_source_sha256':sha(original),'generated_source_sha256':sha(fixed)}
 def run(cmd,label,timeout=240):
  cmd=list(map(str,cmd))
  try:
   p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  path=out/(label+'.log');path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':code,'log':str(path.relative_to(ROOT))});print(kind,label,code,log[-10000:] if code or label=='runtime' else '',flush=True);assert code==0,label;return log
 try:
  cxx='g++' if kind=='asan' else '/usr/bin/aarch64-linux-gnu-g++';flags=prior['flags'];objects=[];sources=[ROOT/'port/matrixgame/native/rangers_campaign_session.cpp',ROOT/'port/matrixgame/native/rangers_data_session.cpp',fixed,probe]
  for source in sources:
   obj=out/(source.stem+'.o');run(['wsl','--exec',cxx,*flags,'-I'+linux(ROOT/'port/matrixgame/native'),'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(obj)
  binary=out/'campaign';manifest={'objects':[linux(ROOT/k) for k in reused]+[linux(p) for p in objects],'output':linux(binary),'report':linux(out/'link-report.json'),'cxx':cxx,'link_flags':['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else [],'libraries':[linux(ROOT/k) for k in prior['libraries_sha256']],'okgf':linux(ROOT/'port/switch/build/e2e5-host-game/switch-effective/okgf/libokgf.so')}
  (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(out/'link-input.json')],'link')
  runtime=out/'runtime';runtime.mkdir();limit=600 if kind=='asan' else 1200
  command=['wsl','--exec','env','--unset=E2E511_HOST_GENERATION_SEED','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
  if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  command+=['timeout','--kill-after=5s',str(limit)+'s']
  if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  command+=[linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)]
  result['stdout']=run(command,'runtime',limit+20)
  assert 'original_generation_complete=PASS stage=8' in result['stdout'] and 'original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN' in result['stdout']
  check_hashes(reused);check_hashes(prior['libraries_sha256'])
  result.update(status='PASS; '+kind.upper()+' ORIGINAL FULL CAMPAIGN/PLAYER/WORKER/CLEANUP; NO BATTLE/GOV RETURN',flags=flags,new_TUs_instrumented=kind=='asan',original_dependencies_instrumented=False,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_campaign_session.hpp',ROOT/'tests/matrixgame_campaign_probe.inc',ROOT/'tools/matrixgame_campaign_overrides.py',ROOT/'tools/e2e_source_overrides.py',Path(__file__).resolve()]},objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},reused_objects_sha256=reused,libraries_sha256=prior['libraries_sha256'],binary=str(binary.relative_to(ROOT)),binary_sha256=sha(binary))
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

"""Real campaign/navigation/Gov/memory snapshot on host sanitizers or ARM64/QEMU."""
import argparse, json, subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_campaign_cleanup_checks import linux, local
from matrixgame_campaign_cleanup_overrides import apply as cleanup
from matrixgame_campaign_snapshot_overrides import apply as snapshot_cleanup

def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('asan','arm64'));kind=parser.parse_args().kind
 base=ROOT/'build/matrixgame/e2e600-campaign-navigation';parent=base/('snapshot-'+kind);parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();old=ROOT/'build/matrixgame/e2e599-campaign-cleanup'/('asan/5' if kind=='asan' else 'arm64/2');prior=read(old/'result.json');assert prior['status'].startswith('PASS');manifest=read(old/'link-input.json')
 hashes={str((ROOT/k).resolve()):v for k,v in {**prior['reused_objects_sha256'],**prior['objects_sha256']}.items()}
 inputs=[local(p) for p in manifest['objects'] if Path(p).name not in ('campaign_probe.o','rangers_campaign_session.o','aGalaxy.o','aShip.o')];reused={str(p.relative_to(ROOT)):hashes[str(p.resolve())] for p in inputs};check_hashes(reused)
 flags=prior['flags'];cxx='g++' if kind=='asan' else '/usr/bin/aarch64-linux-gnu-g++'
 win=base/'snapshot-windows/4';reference=read(win/'result.json');assert reference['status'].startswith('PASS');check_hashes(reference['sources_sha256'])
 source=out/'snapshot_probe.cpp';source.write_text((win/'navigation_probe.cpp').read_text(encoding='utf-8'),encoding='utf-8')
 sources=[ROOT/'port/matrixgame/native/rangers_campaign_session.cpp',ROOT/'port/matrixgame/native/rangers_achievement_session.cpp',source]
 mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json');originals={}
 for name in ('aGalaxy.cpp','aShip.cpp'):
  original=ROOT/mapping['upstream/cpp/src/'+name];fixed=out/name;fixed.write_text(snapshot_cleanup(name,cleanup(name,original.read_text(encoding='utf-8'))),encoding='utf-8');sources.append(fixed);originals[str(original.relative_to(ROOT))]=sha(original)
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','battle_Gov_return':'NOT RUN','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_campaign_session.hpp',ROOT/'port/matrixgame/native/rangers_achievement_session.hpp',ROOT/'tools/matrixgame_campaign_snapshot_overrides.py',ROOT/'tools/e2e_source_overrides.py',Path(__file__).resolve()]},'original_source_sha256':originals,'reused_objects_sha256':reused,'prior_result_sha256':sha(old/'result.json'),'reference_result_sha256':sha(win/'result.json'),'flags':flags}
 def run(command,label,timeout=600):
  command=list(map(str,command))
  try:
   p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':code})
  shown='\n'.join(line for line in log.splitlines() if line.startswith(('CAMPAIGN','NAVIGATION','GOVERNMENT','SNAPSHOT'))) if code==0 and label=='runtime' else log[-10000:] if code else ''
  print(kind,label,code,shown,flush=True)
  if label=='runtime':result['stdout']=log
  assert code==0,label
 try:
  objects=[]
  for source in sources:
   obj=None
   for j in range(i-1,0,-1):
    path=parent/str(j)/'result.json'
    if not path.is_file():continue
    saved=read(path)
    if saved['flags']!=flags:continue
    candidates=[(ROOT/k,v) for k,v in saved.get('objects_sha256',{}).items() if Path(k).name==source.stem+'.o']
    previous_source=parent/str(j)/source.name if source.parent==out else source
    previous_hash=saved.get('sources_sha256',{}).get(str(previous_source.relative_to(ROOT)))
    if candidates and previous_hash==sha(source) and sha(candidates[0][0])==candidates[0][1]:obj=candidates[0][0];break
   if obj is None:obj=out/(source.stem+'.o');run(['wsl','--exec',cxx,*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem)
   else:print(kind,'reuse-'+source.stem,flush=True)
   objects.append(obj);result['objects_sha256']={str(p.relative_to(ROOT)):sha(p) for p in objects}
  binary=out/'snapshot';manifest.update(objects=[linux(p) for p in [*inputs,*objects]],output=linux(binary),report=linux(out/'link-report.json'));path=out/'link-input.json';path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
  run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(path)],'link');result.update(binary_sha256=sha(binary),objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects})
  runtime=out/'runtime';runtime.mkdir();command=['wsl','--exec','env','--unset=E2E511_HOST_GENERATION_SEED','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
  if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:fast_unwind_on_malloc=0:malloc_context_size=40','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  command+=['timeout','--kill-after=5s','1200s']
  if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  command += [linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)];run(command,'runtime',1220)
  assert 'SNAPSHOT original_memory_roundtrip=PASS' in result['stdout'] and 'original_world_worker_cleanup=PASS' in result['stdout'];check_hashes(result['sources_sha256']);check_hashes(reused)
  result['status']='PASS; '+kind.upper()+' ORIGINAL CAMPAIGN/NAVIGATION/GOV/MEMORY SNAPSHOT; NO BATTLE/GOV RETURN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

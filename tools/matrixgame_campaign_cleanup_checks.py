"""Recheck full original campaign after proven cleanup fixes; retain leak failures."""
import json,subprocess
from pathlib import Path
from matrixgame_campaign_cleanup_overrides import apply
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e599-campaign-cleanup'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def local(p):
 assert p.startswith('/mnt/d/'),p
 return Path('D:/'+p[len('/mnt/d/'):])
def main():
 old=ROOT/'build/matrixgame/e2e598-campaign/asan/1';baseline=read(old/'result.json');assert baseline['status']=='INCOMPLETE' and baseline['commands'][-1]['exit']==1
 previous=read(old/'link-input.json');flags=read(ROOT/'build/matrixgame/e2e597-global-ui/asan/1/result.json')['flags'];mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json')
 parent=BASE/'asan';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();units=('aGalaxy.cpp','aPlanet.cpp','aPlayer.cpp','EC_Ether.cpp','aRanger.cpp','aGalaxyEvent.cpp','EC_Expression.cpp')
 history=[(parent/str(j),read(parent/str(j)/'result.json')) for j in range(i-1,0,-1)]
 reused={str(local(p).relative_to(ROOT)):sha(local(p)) for p in previous['objects'] if Path(p).name not in (*tuple(Path(n).stem+'.o' for n in units),'rangers_script_session.o')}
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','battle_Gov_return':'NOT RUN','attempt':i,'reused_objects_sha256':reused,'baseline_result_sha256':sha(old/'result.json'),'flags':flags,'new_TUs_instrumented':True,'all_original_dependencies_instrumented':False,'lifetime_header_sha256':sha(ROOT/'port/switch/platform/script_array_lifetime.hpp')}
 def run(cmd,label,timeout=600):
  cmd=list(map(str,cmd))
  try:
   p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  path=out/(label+'.log');path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':code,'log':str(path.relative_to(ROOT))});print(label,code,log[-13000:] if code or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  assert code==0,label;return log
 try:
  objects=[];sources={};originals={}
  for name in units:
   original=ROOT/mapping['upstream/cpp/src/'+name];source=out/name;text=apply(name,original.read_text(encoding='utf-8'));source.write_text(text,encoding='utf-8');originals[str(original.relative_to(ROOT))]=sha(original);sources[str(source.relative_to(ROOT))]=sha(source)
   previous_object=None
   for directory,saved in history:
    if saved['flags']!=flags or not (directory/name).is_file() or sha(directory/name)!=sha(source):continue
    if name=='EC_Expression.cpp' and saved.get('lifetime_header_sha256')!=result['lifetime_header_sha256']:continue
    candidates=[(ROOT/k,v) for k,v in saved.get('objects_sha256',{}).items() if Path(k).name==source.stem+'.o']
    if candidates and sha(candidates[0][0])==candidates[0][1]:previous_object=candidates[0][0];break
    if any(c['label']=='compile-'+source.stem and c['exit']==0 for c in saved['commands']) and (directory/(source.stem+'.o')).is_file():previous_object=directory/(source.stem+'.o');break
   if previous_object:
    obj=previous_object;print('reuse-unchanged-'+source.stem,flush=True)
   else:
    obj=out/(source.stem+'.o');run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem)
   objects.append(obj)
   result.update(original_source_sha256=originals,generated_source_sha256=sources,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects})
  source=ROOT/'port/matrixgame/native/rangers_script_session.cpp';obj=out/'rangers_script_session.o'
  run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-rangers_script_session');objects.append(obj)
  sources[str(source.relative_to(ROOT))]=sha(source);sources['port/switch/platform/script_array_lifetime.hpp']=sha(ROOT/'port/switch/platform/script_array_lifetime.hpp')
  check_hashes(reused);result.update(original_source_sha256=originals,generated_source_sha256=sources,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},overlay_sha256=sha(ROOT/'tools/matrixgame_campaign_cleanup_overrides.py'),generator_sha256=sha(ROOT/'tools/e2e_source_overrides.py'))
  binary=out/'campaign';manifest={**previous,'objects':[linux(ROOT/k) for k in reused]+[linux(p) for p in objects],'output':linux(binary),'report':linux(out/'link-report.json')};manifest_path=out/'link-input.json';manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
  run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(manifest_path)],'link');result.update(binary=str(binary.relative_to(ROOT)),binary_sha256=sha(binary))
  runtime=out/'runtime';runtime.mkdir();command=['wsl','--exec','env','--unset=E2E511_HOST_GENERATION_SEED','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:fast_unwind_on_malloc=0:malloc_context_size=40','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1','timeout','--kill-after=5s','600s',linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)]
  run(command,'runtime',620);check_hashes(reused);check_hashes(sources);check_hashes(originals)
  assert 'original_generation_complete=PASS stage=8' in result['stdout'] and 'original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN' in result['stdout']
  result['status']='PASS; HOST ASAN/UBSAN ORIGINAL FULL CAMPAIGN WITH CLEANUP FIXES; NO BATTLE/GOV RETURN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

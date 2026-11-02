"""Verify campaign cleanup changes on Windows reference or ARM64/QEMU; preserve prior gates."""
import argparse, json, subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_campaign_cleanup_checks import BASE, linux, local
from matrixgame_campaign_cleanup_overrides import apply

UNITS=('aGalaxy.cpp','aPlanet.cpp','aPlayer.cpp','EC_Ether.cpp','aRanger.cpp','aGalaxyEvent.cpp','EC_Expression.cpp')

def main():
 parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','arm64'));kind=parser.parse_args().kind
 windows=kind=='windows';parent=BASE/kind;parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir()
 old=ROOT/'build/matrixgame/e2e598-campaign'/('windows-asan/5' if windows else 'arm64/1');prior=read(old/'result.json');assert prior['status'].startswith('PASS')
 flags=prior['flags'];mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json')
 suffix='.obj' if windows else '.o';replaced={Path(n).stem+suffix for n in UNITS}|{'rangers_script_session'+suffix}
 if windows:
  from matrixgame_windows_host_env import environment, compiler
  env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy');env.pop('E2E511_HOST_GENERATION_SEED',None)
  inputs=[Path(line.strip('"')) for line in (old/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name not in replaced]
  original_hashes=prior['link_inputs_sha256'];reused={str(p):original_hashes[str(p)] for p in inputs}
 else:
  env=None;manifest=read(old/'link-input.json');inputs=[local(p) for p in manifest['objects'] if Path(p).name not in replaced]
  original_hashes={str(Path(k)):v for k,v in {**prior['reused_objects_sha256'],**prior['objects_sha256']}.items()};reused={str(p.relative_to(ROOT)):original_hashes[str(p.relative_to(ROOT))] for p in inputs}
  check_hashes(prior['libraries_sha256'])
 check_hashes(reused)
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','battle_Gov_return':'NOT RUN','prior_result_sha256':sha(old/'result.json'),'reused_objects_sha256':reused,'flags':flags,'sources_sha256':{},'objects_sha256':{}}
 def run(command,label,timeout=600,cwd=ROOT):
  command=list(map(str,command))
  try:
   p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':code});print(kind,label,code,log[-10000:] if code or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  assert code==0,label
 try:
  sources=[]
  for name in UNITS:
   original=ROOT/'build/matrixgame/e2e577-rangers-windows/source'/name if windows else ROOT/mapping['upstream/cpp/src/'+name]
   source=out/name;source.write_text(apply(name,original.read_text(encoding='utf-8')),encoding='utf-8');sources.append(source)
   result['sources_sha256'][str(original.relative_to(ROOT))]=sha(original)
  sources.append(ROOT/'port/matrixgame/native/rangers_script_session.cpp');objects=[]
  for source in sources:
   obj=out/(source.stem+suffix)
   command=[compiler(env),*flags,'/c',source,'/Fo'+str(obj)] if windows else ['wsl','--exec','/usr/bin/aarch64-linux-gnu-g++',*flags,'-c',linux(source),'-o',linux(obj)]
   run(command,'compile-'+source.stem);objects.append(obj);result['sources_sha256'][str(source.relative_to(ROOT))]=sha(source);result['objects_sha256'][str(obj.relative_to(ROOT))]=sha(obj)
  result['sources_sha256']['port/switch/platform/script_array_lifetime.hpp']=sha(ROOT/'port/switch/platform/script_array_lifetime.hpp')
  binary=out/('campaign.exe' if windows else 'campaign')
  if windows:
   rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*inputs,*objects]),encoding='utf-8')
   command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:]
   command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else ('/MAP:'+str(out/'campaign.map')) if s.startswith('/MAP:') else s for s in command];run(command,'link')
   env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH']
  else:
   manifest.update(objects=[linux(p) for p in [*inputs,*objects]],output=linux(binary),report=linux(out/'link-report.json'));path=out/'link-input.json';path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
   run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(path)],'link')
  result['binary_sha256']=sha(binary);runtime=out/'runtime';runtime.mkdir()
  if windows:run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],'runtime',600,out)
  else:run(['wsl','--exec','env','--unset=E2E511_HOST_GENERATION_SEED','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','timeout','--kill-after=5s','1200s','qemu-aarch64','-L','/usr/aarch64-linux-gnu',linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)],'runtime',1220)
  assert 'original_generation_complete=PASS stage=8' in result['stdout'] and 'original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN' in result['stdout']
  check_hashes(reused);check_hashes(result['sources_sha256']);check_hashes(result['objects_sha256'])
  result['status']='PASS; '+('PARTIAL WINDOWS ASAN' if windows else 'ARM64/QEMU')+' ORIGINAL FULL CAMPAIGN WITH CLEANUP FIXES; NO BATTLE/GOV RETURN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

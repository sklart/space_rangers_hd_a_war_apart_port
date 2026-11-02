"""Verify a genuine random-world UBSan failure without changing its RNG draws."""
import json, subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_campaign_cleanup_checks import linux, local
from matrixgame_campaign_revolution_overrides import apply

def main():
 base=ROOT/'build/matrixgame/e2e601-revolution-seed';base.mkdir(parents=True,exist_ok=True);i=1
 while (base/str(i)).exists():i+=1
 out=base/str(i);out.mkdir();old=ROOT/'build/matrixgame/e2e600-campaign-navigation/snapshot-asan/2';prior=read(old/'result.json');assert prior['status']=='INCOMPLETE' and prior['commands'][-1]['exit']==1
 manifest=read(old/'link-input.json');flags=prior['flags'];inputs=[local(p) for p in manifest['objects'] if Path(p).name!='aPlanet.o'];assert len(inputs)==len(manifest['objects'])-1
 baseline={str((ROOT/k).resolve()):v for k,v in {**prior['reused_objects_sha256'],**prior['objects_sha256']}.items()};reused={str(p.relative_to(ROOT)):baseline[str(p.resolve())] for p in inputs};check_hashes(reused)
 original=ROOT/'build/matrixgame/e2e599-campaign-cleanup/asan/3/aPlanet.cpp';source=out/'aPlanet.cpp';source.write_text(apply('aPlanet.cpp',original.read_text(encoding='utf-8')),encoding='utf-8');obj=out/'aPlanet.o';binary=out/'snapshot'
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','battle_Gov_return':'NOT RUN','production_generator_integration':'PENDING; E600 ARM64 source fingerprints preserved while running','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in (source,original,ROOT/'tools/matrixgame_campaign_revolution_overrides.py',Path(__file__).resolve())},'reused_objects_sha256':reused,'prior_result_sha256':sha(old/'result.json')}
 def run(command,label,timeout=600):
  command=list(map(str,command))
  try:
   p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':code});print(label,code,log[-9000:] if code else '\n'.join(l for l in log.splitlines() if l.startswith(('SNAPSHOT','CAMPAIGN','GOVERNMENT','NAVIGATION'))),flush=True)
  if label=='runtime':result['stdout']=log
  assert code==0,label
 try:
  # Extract the actual patched expression. Independent arbitrary-precision
  # oracle uses truncation toward zero before multiplication and low32 wrap.
  text=source.read_text(encoding='utf-8');start=text.index('    void TPlanet::TriggerGovernmentRevolution() {');end=text.index('    void TPlanet::TryTriggerEconomicEvent() {',start)
  expression='aGalaxy::Galaxy->CurrentTurn / 10 * GenerationSeed';assert text[start:end].count(expression)==1
  expression=expression.replace('aGalaxy::Galaxy->CurrentTurn','turn').replace('GenerationSeed','seed')
  lines=['#include <cstdint>','constexpr std::int32_t actual(std::int32_t turn,std::uint32_t seed){return '+expression+';}'];count=0
  for turn in (-2147483648,-301,-1,0,1,2,30,40,300,301,2147483647):
   for seed in (0,1,0x7fffffff,0x80000000,0xffffffff,0x617c9273,1069127773):
    value=(((abs(turn)//10)*(1 if turn>=0 else -1))*seed)&0xffffffff
    if value>=0x80000000:value-=0x100000000
    lines.append(f'static_assert(actual({turn},{seed}u)=={value});');count+=1
  oracle=out/'seed-parity.cpp';oracle.write_text('\n'.join(lines)+'\n',encoding='utf-8')
  run(['wsl','--exec','g++','-std=gnu++20','-c',linux(oracle),'-o',linux(out/'seed-host.o')],'seed-host')
  run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe','-std=gnu++20','-c',oracle,'-o',out/'seed-sdk.o'],'seed-sdk');result.update(seed_checks=count,oracle_source_sha256=sha(oracle))
  run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-aPlanet');result['object_sha256']=sha(obj)
  manifest.update(objects=[linux(p) for p in [*inputs,obj]],output=linux(binary),report=linux(out/'link-report.json'));path=out/'link-input.json';path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
  run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(path)],'link');result['binary_sha256']=sha(binary)
  runtime=out/'runtime';runtime.mkdir();run(['wsl','--exec','env','--unset=E2E511_HOST_GENERATION_SEED','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:fast_unwind_on_malloc=0:malloc_context_size=40','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1','timeout','--kill-after=5s','1200s',linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)],'runtime',1220)
  assert 'SNAPSHOT original_memory_roundtrip=PASS' in result['stdout'] and 'original_world_worker_cleanup=PASS' in result['stdout'];check_hashes(reused);check_hashes(result['sources_sha256'])
  result['status']='PASS; HOST ASAN/UBSAN/LSAN ORIGINAL CAMPAIGN/NAVIGATION/GOV/MEMORY SNAPSHOT; REVOLUTION SEED CANDIDATE; NO BATTLE RETURN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

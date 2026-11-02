"""Compile actual generated seed expressions against independent Python low32 oracles."""
import json,re,subprocess
from matrixgame_campaign_cleanup_overrides import apply
from matrixgame_standard_nro_audit import ROOT,sha,read
def main():
 base=ROOT/'build/matrixgame/e2e599-campaign-cleanup/seed-parity';base.mkdir(parents=True,exist_ok=True)
 original=ROOT/'upstream/cpp/src/aPlanet.cpp';text=apply('aPlanet.cpp',original.read_text(encoding='utf-8'))
 begin=text.index('    void TPlanet::TryTriggerEconomicEvent() {');end=text.index('\n    }\n',begin);body=text[begin:end]
 expressions=re.findall(r'aGalaxy::Galaxy->CurrentTurn \* GenerationSeed \* (\d+)',body);assert len(expressions)==15
 guard='static_cast<std::int32_t>(static_cast<std::uint32_t>(aGalaxy::Galaxy->CurrentTurn) + GenerationSeed) % 30';assert body.count(guard)==1
 turns=(-2147483648,-301,-1,0,1,2,30,300,301,2147483647);seeds=(0,1,0x7fffffff,0x80000000,0xffffffff,0x617c9273);lines=['#include <cstdint>'];count=0
 def small(expression):return expression.replace('aGalaxy::Galaxy->CurrentTurn','turn').replace('GenerationSeed','seed')
 for index,factor in enumerate(expressions):
  expression='aGalaxy::Galaxy->CurrentTurn * GenerationSeed * '+factor
  lines.append('constexpr std::uint32_t seed_'+str(index)+'(std::int32_t turn,std::uint32_t seed){return '+small(expression)+';}')
  for turn in turns:
   for seed in seeds:
    expected=(turn*seed*int(factor))&0xffffffff;lines.append(f'static_assert(seed_{index}({turn},{seed}u)=={expected}u);');count+=1
 lines.append('constexpr std::int32_t event_guard(std::int32_t turn,std::uint32_t seed){return '+small(guard)+';}')
 for turn in turns:
  for seed in seeds:
   signed=(turn+seed)&0xffffffff
   if signed>=0x80000000:signed-=0x100000000
   quotient=(abs(signed)//30)*(1 if signed>=0 else -1);expected=signed-quotient*30
   lines.append(f'static_assert(event_guard({turn},{seed}u)=={expected});');count+=1
 source=base/'seed_parity.cpp';rendered='\n'.join(lines)+'\n'
 if source.exists():assert source.read_text(encoding='utf-8')==rendered
 else:source.write_text(rendered,encoding='utf-8')
 result={'status':'INCOMPLETE','checks':count,'scope':'PURE GENERATED RNG EXPRESSIONS; NO GAMEPLAY OR RNG DRAWS EXECUTED','commands':[]}
 try:
  for kind,command in (('host',['wsl','--exec','g++','-std=gnu++20','-c',str(source).replace('\\','/').replace('D:/','/mnt/d/'),'-o',str(base/'host.o').replace('\\','/').replace('D:/','/mnt/d/')]),('sdk',['C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe','-std=gnu++20','-c',source,'-o',base/'sdk.o'])):
   command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(base/(kind+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'command':command,'exit':p.returncode});assert not p.returncode,p.stdout+p.stderr
  result.update(status='PASS; 960 GENERATED SEED/GUARD EXPRESSIONS MATCH INDEPENDENT LOW32 ORACLES ON HOST/SDK',source_sha256=sha(source),original_sha256=sha(original),overlay_sha256=sha(ROOT/'tools/matrixgame_campaign_cleanup_overrides.py'),objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in (base/'host.o',base/'sdk.o')})
 finally:
  path=base/'result.json';i=1
  while path.exists():path=base/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(result['status'])
if __name__=='__main__':main()

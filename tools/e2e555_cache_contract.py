"""Real GI hot acquire + frozen eviction contract; no GUI/hardware causality claim."""
import hashlib,json,mmap,os,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
BASE=ROOT/'build/e2e555-cache-contract'
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tests')]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 BASE.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'hardware':'NOT RUN','production_policy_changed':False}
 def run(cmd,label,expected=0,env=None):
  p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,env=env);log=p.stdout+p.stderr
  (BASE/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'expected_exit':expected});print(label,p.returncode,log[-2300:] if p.returncode!=expected else '',flush=True);assert p.returncode==expected,label
  return log
 try:
  prior=ROOT/'build/e2e523/cache-retained';frozen=ROOT/'port/switch/build/e2e-game-perf515/generated/EC_Cache.cpp';text=(prior/'fixture.cpp').read_text();assert sha(prior/'fixture.cpp')=='2fe38d570ebb41dd33e2b94cd436deb5ca3ad4f221d7eb31460f1a03478928fd'
  start=text.index('  auto* manager=pas::construct_call<EC_Cache::TCacheEC>');end=text.index('\n  pas::free(cache);',start)
  text=text[:start]+(ROOT/'tests/e2e555_cache_contract.inc').read_text()+text[end:]
  text='#include "types/EC_Data.hpp"\n#include "units/SysUtilsImports.hpp"\n#include "units/WindowsImports.hpp"\n'+text
  # Unused cold-load endpoint must abort: this test proves the hot branch only.
  text+='\nnamespace EC_Data {void TDataEC::ReadBufferByPath(const pas::WideString&,EC_Buf::TBufEC*){std::abort();}}\nnamespace GR_DX {void EvictTextureCaches(std::uint8_t force){assert(force);}}\n'
  text+='\nnamespace SysUtilsImports {void PAS_STDCALL Sleep(std::uint32_t){std::abort();}}\nnamespace WindowsImports {std::uint32_t PAS_STDCALL CreateEvent(void*,std::int32_t,std::int32_t,std::uint8_t*){std::abort();}std::int32_t PAS_STDCALL CloseHandle(std::uint32_t){std::abort();}}\nnamespace WindowsSdk {BOOL PAS_STDCALL SetEvent(THandle){std::abort();}}\n'
  fixture=BASE/'fixture.cpp';fixture.write_text(text,encoding='utf-8')
  assert sha(frozen)=='48a3c5114244762a89e121db3f2547511beba76720d4e8d44ebb9497885a018c'
  flags=['-std=gnu++20','-O1','-g','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-invalid offsetof'.replace(' ','-')]
  includes=['-I'+str(ROOT/p) for p in ['port/switch/platform','upstream/cpp/src','upstream/cpp/runtime','upstream/okgf/include']]
  # The isolated TU also defines unused queues for unrelated cache subclasses.
  # vptr metadata would retain their unavailable whole-game RTTI at link time.
  obj=BASE/'frozen-cache.o';run(['g++',*flags,'-fno-sanitize=vptr',*includes,'-c',frozen,'-o',obj],'compile-frozen-cache')
  cmd=json.loads((prior/'result.json').read_text())['link_command'];cmd=[str(fixture) if s.endswith('/cache-retained/fixture.cpp') else str(BASE/'cache-contract') if s.endswith('/cache-retained/cachegi') else str(obj) if s.endswith('/ruins-resource/EC_Cache.o') else s for s in cmd];cmd[1:1]=flags[1:];run(cmd,'link')
  from e2e5_formgameset2_compare import entries,payload
  from probe_gai_release import decode2,fp
  pkg=ROOT/'windows/Space Rangers HD A War Apart/DATA/background.pkg'
  with pkg.open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as blob:
   rows=[row for row in entries(blob) if row[0].casefold()=='data/formruinsbg/2rcbg.gi'];assert len(rows)==1;encoded=payload(blob,rows[0])
  metadata,pixels=decode2(encoded);assert metadata['decoded']['width']==1920 and metadata['decoded']['height']==1536
  (BASE/'resource.gi').write_bytes(encoded);(BASE/'decoded.bgra').write_bytes(pixels)
  base=[BASE/'cache-contract',BASE/'decoded.bgra','e36b6f814bc16840','c7abbe2c80d5e0f4',BASE/'resource.gi',fp(pixels)['fnv64']]
  report['observations']={}
  for case in ['retained-peer','caller-retained','wrong-class','bound-wrong-class','trim','uaf']:
   log=run(base,case,86 if case=='uaf' else 0,dict(os.environ,E2E555_CASE=case,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:exitcode=86',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
   assert 'acquired_through_original_API=1 nested_retains=2' in log
   if case=='uaf':assert 'AddressSanitizer: heap-use-after-free' in log
   report['observations'][case]=log
  report.update(status='PASS; FROZEN REAL GI HOT ACQUIRE RETAIN/EVICT CONTRACT OBSERVED; EXPECTED UAF REPRODUCED',resource={'path':'DATA/FormRuinsBG/2RCbg.gi','package_sha256':sha(pkg),'encoded_sha256':sha(BASE/'resource.gi'),'decoded_sha256':sha(BASE/'decoded.bgra')},binary_sha256=sha(BASE/'cache-contract'),frozen_cache_sha256=sha(frozen))
 finally:
  report['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in [pathlib.Path(__file__),ROOT/'tests/e2e555_cache_contract.inc',BASE/'fixture.cpp'] if p.exists()};(BASE/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

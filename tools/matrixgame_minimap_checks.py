"""Addressed original minimap cache/scopes and full SDK link; no NRO."""
import argparse,hashlib,json,os,subprocess,time,uuid,sys,shlex
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from matrixgame_minimap_cache_prepare import prepare
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e528-minimap';SOURCE=BASE/'source';FIXTURE=BASE/'fixture'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['prepare','sdk','host','arm64']);kind=ap.parse_args().kind;out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 def run(cmd,label,env=None):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-3000:] if label!='symbols' else 'saved',flush=True);assert p.returncode==0;return p.stdout
 try:
  if kind=='prepare':run([sys.executable,ROOT/'tools/matrixgame_engine_prepare.py','--output',SOURCE],'prepare');j['extraction']=prepare(SOURCE,FIXTURE)
  elif kind=='sdk':
   cmd=json.loads((ROOT/'build/matrixgame/e2e524-text/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');j['strong_undefined']=[line for line in nm.splitlines() if ' U ' in line];assert not j['strong_undefined'];j['elf_sha256']=sha(elf)
   for symbol in ('offscreen::SurfaceLockScope::SurfaceLockScope(', 'CMinimap::RenderBackground(', 'CMinimap::RestoreTexture(', 'CGame::Init('):assert symbol in nm,symbol
   j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  else:
   sources=['tests/test_matrixgame_minimap_cache.cpp',str(FIXTURE.relative_to(ROOT)/'OriginalMinimapCache.cpp'),str(FIXTURE.relative_to(ROOT)/'OriginalCachePrimitives.cpp'),'port/matrixgame/native-core/texture_mips.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_texture.cpp','platform_3g.cpp','legacy_texture_backend.cpp','unmanaged_texture_backend.cpp','native_surfaces.cpp','offscreen_backend.cpp','offscreen_scope.cpp','output_paths_backend.cpp','platform_globals.cpp')]+[str(SOURCE.relative_to(ROOT)/'MatrixLib/3G'/n) for n in ('OriginalManagedTexture.cpp','OriginalUnmanagedTexture.cpp')]
   prior=json.loads((ROOT/('build/matrixgame/e2e522/unions-'+kind+'/result.json')).read_text());cmd=[s for s in prior['commands'][0]['command'] if not s.endswith('.cpp') and not s.endswith('libmatrixgame-native-cache.a') and not s.endswith('libmatrixgame-native-vector-object.a')];cmd=[s.replace('/build/matrixgame/e2e521/source','/build/matrixgame/e2e528-minimap/source') for s in cmd];cmd.insert(3,'-I'+linux(FIXTURE));first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/p) for p in sources];cmd[cmd.index('-o')+1]=linux(out/'minimap-cache')
   # Keep per-TU objects/dependencies: a linker/harness fix does not need a
   # repeated whole compilation. Cache hits require matching dependency hashes.
   object_dir=out/'objects';object_dir.mkdir(exist_ok=True);flags=[];skip=False
   for arg in cmd[3:]:
    if skip:skip=False;continue
    if arg=='-o':skip=True;continue
    if arg.endswith(('.cpp','.a','.so')) or arg.startswith(('-l','-Wl,')) or arg=='-no-pie':continue
    flags.append(arg)
   def dependency_hashes(dep):
    text=dep.read_text().replace('\\\n',' ');paths=shlex.split(text.split(':',1)[1]);result={}
    for p in paths:
     if not p.startswith('/mnt/d/'):return None
     native=Path('D:/'+p[7:]);result[str(native)]=sha(native)
    return result
   def compile_source(source):
    obj=object_dir/(Path(source).stem+'.o');dep=obj.with_suffix('.d');meta=obj.with_suffix('.json');command=cmd[:3]+flags+['-MMD','-MF',linux(dep),'-c',linux(ROOT/source),'-o',linux(obj)]
    if meta.exists() and obj.exists() and dep.exists():
     previous=json.loads(meta.read_text());deps=dependency_hashes(dep)
     if deps is not None and previous=={'command':command,'dependencies':deps,'object_sha256':sha(obj)}:
      print('cached',source,flush=True);return Path(source).as_posix(),linux(obj)
    run(command,'build-'+Path(source).stem);deps=dependency_hashes(dep);meta.write_text(json.dumps({'command':command,'dependencies':deps,'object_sha256':sha(obj)},indent=2)+'\n');return Path(source).as_posix(),linux(obj)
   with ThreadPoolExecutor(max_workers=4) as executor:objects=dict(executor.map(compile_source,sources))
   cmd=[objects.get(Path(s.replace('/mnt/d/','D:/')).relative_to(ROOT).as_posix(),s) if s.endswith('.cpp') else s for s in cmd]
   run(cmd,'link')
   output=out/('run-'+uuid.uuid4().hex);cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   j['stdout']=run(cmd+[linux(out/'minimap-cache'),linux(output)],'runtime');j['output_sha256']={str(p.relative_to(ROOT)):sha(p) for p in output.rglob('*') if p.is_file()};assert len(j['output_sha256'])==1
   j['extraction']=json.loads((FIXTURE/'extraction.json').read_text())
  paths=['port/matrixgame/native-engine/'+p for p in ('minimap_lifetime.hpp','offscreen_scope.hpp','offscreen_scope.cpp','native_texture.hpp','native_texture.cpp','native_surfaces.hpp','native_surfaces.cpp')]+['tools/matrixgame_minimap_overrides.py','tools/matrixgame_minimap_checks.py','tools/matrixgame_minimap_cache_prepare.py','tests/test_matrixgame_minimap_cache.cpp','tests/test_matrixgame_native_surfaces.cpp']
  if kind not in ('sdk','prepare'):paths+=sources+[str(FIXTURE.relative_to(ROOT)/'MinimapFixture.hpp')]
  j['source_sha256']={p:sha(ROOT/p) for p in paths};j['Switch_hardware']='HARDWARE_PENDING';j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()

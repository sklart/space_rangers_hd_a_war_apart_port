"""Compile original Rangers and portable services for a new MSVC reference."""
import concurrent.futures,hashlib,json,shutil,subprocess
from pathlib import Path
from e2e_runtime_overrides import prepare_runtime_overlay
from e2e_dat_cleanup import apply as dat_cleanup
from matrixgame_windows_host_env import environment,compiler
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 out=BASE/'rangers-windows-2'
 if out.exists():raise RuntimeError('Rangers reference output already exists')
 out.mkdir();env=environment(out);env['VSLANG']='1033';result={'status':'INCOMPLETE','compiled':[],'failures':[]}
 try:
  source_root=out/'source';source_root.mkdir()
  upstream=ROOT/'upstream/cpp/src';overlay=ROOT/'port/switch/build/e2e5-host-game/reproducible/source-overlay'
  for tree in (upstream,overlay):
   for path in tree.rglob('*.hpp'):
    dest=source_root/path.relative_to(tree);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
  runtime=out/'runtime-overlay';prepare_runtime_overlay(ROOT/'upstream/cpp/runtime',runtime,portable_windows_host=True)
  for path in runtime.glob('*.hpp'):shutil.copyfile(path,source_root/path.name)
  report=json.loads((ROOT/'port/switch/build/e2e5-host-game/reproducible/report.json').read_text(encoding='utf8'))
  image=BASE/'image-libs';image_source=BASE/'source-deps-retry';audio=BASE/'audio-libs/prefix'
  sdl=BASE/'deps/sdl2/SDL2-2.30.11';jpeg=BASE/'source-deps/libjpeg-turbo-3.1.0/src'
  shutil.copytree(sdl/'include',out/'SDL2')
  includes=[source_root,runtime,ROOT/'port/switch/platform',ROOT/'upstream/okgf/include',out,sdl/'include',audio/'include',
      image_source/'libpng-1.6.48',image/'png',jpeg,image/'jpeg',image_source/'zlib-1.3.2',image/'zlib',
      ROOT/'port/matrixgame/native',ROOT/'port/matrixgame/native-core',BASE/'native-windows/core/source',ROOT/'port/matrixgame/native-engine']
  flags=['/nologo','/std:c++20','/EHsc','/utf-8','/MT','/O2','/UNDEBUG','/DNOMINMAX','/D_CRT_SECURE_NO_WARNINGS','/DSDL_MAIN_HANDLED']
  flags += ['/D'+v[2:] for v in report['compile_flags'] if v.startswith('-D')]
  flags += ['/I'+str(p) for p in includes]
  units=[];source_hashes={}
  for unit in report['compiled_units']:
   if unit['original']=='tools/e2e5_host_game_main.cpp':continue
   original=unit['original'];source=ROOT/unit['effective'].replace('\\','/')
   assert source.is_file(),source;source_hashes[unit['effective']]=sha(source)
   category='original' if original.startswith('upstream/') else 'platform'
   if category=='original':
    if source.name=='runtime.cpp':source=runtime/'runtime.cpp'
    content=source.read_text(encoding='utf8').replace('__attribute__((noinline))','__declspec(noinline)')
    if source.name in ('EC_BlockPar.cpp','EC_Data.cpp') and 'resource_scope' not in content:content=dat_cleanup(content,source.name)
    dest=source_root/source.name;dest.write_text(content,encoding='utf8');source=dest
   units.append((source,out/'objects'/category/(Path(original).stem+'.obj')))
  for name in ('rangers_bridge.cpp','engine_callbacks.cpp','rangers_host_callbacks.cpp','rangers_audio_session.cpp',
               'rangers_music_session.cpp','rangers_data_session.cpp','rangers_host_session.cpp','rangers_script_session.cpp',
               'rangers_world_shutdown.cpp','rangers_global_ui_session.cpp','rangers_ui_context.cpp','rangers_achievement_session.cpp',
               'rangers_campaign_session.cpp'):
   units.append((ROOT/'port/matrixgame/native'/name,out/'objects/native'/(Path(name).stem+'.obj')))
  assert len(units)==343
  result['flags']=flags;result['effective_inputs_sha256']=source_hashes
  def compile_one(unit):
   source,obj=unit;obj.parent.mkdir(parents=True,exist_ok=True);cmd=[str(compiler(env)),*flags,'/c',str(source),'/Fo'+str(obj)]
   p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
   log=out/'logs'/obj.parent.name/(obj.stem+'.log');log.parent.mkdir(parents=True,exist_ok=True);log.write_text(p.stdout+p.stderr,encoding='utf8')
   return {'source':str(source.relative_to(ROOT)),'object':str(obj.relative_to(ROOT)),'exit':p.returncode,'log':str(log.relative_to(ROOT)),**({'sha256':sha(obj)} if p.returncode==0 else {})}
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
   for index,item in enumerate(pool.map(compile_one,units),1):
    result['compiled'].append(item)
    if item['exit']:result['failures'].append(item);print('FAILED',item['source'],flush=True)
    if index%25==0:print('compiled',index,'/',len(units),'failures',len(result['failures']),flush=True)
  if result['failures']:raise RuntimeError('one or more translation units failed')
  result['status']='PASS; MSVC Rangers objects only; link and runtime NOT RUN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

"""Native Windows LLP64 reference build of the real hosted Rangers services.

Private source copies are required by MSVC quoted-include stack lookup. They
retain the effective original unit bytes; no upstream or frozen input is edited.
"""
import concurrent.futures
import json
import shutil
import subprocess
from pathlib import Path
from e2e_runtime_overrides import prepare_runtime_overlay
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment,compiler,CMAKE,NINJA
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows'
GAME=ROOT/'windows/Space Rangers HD A War Apart'

def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE)
 env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
 env['VSLANG']='1033'
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 if (BASE/'result.json').exists():
  index=1
  while (BASE/f'result-attempt-{index}.json').exists():index+=1
  shutil.copyfile(BASE/'result.json',BASE/f'result-attempt-{index}.json')
 deps=read(BASE/'deps/result.json');assert deps['status'].startswith('PASS')
 for name,digest in {**deps['libraries_sha256'],**deps['headers_sha256']}.items():assert sha(ROOT/name)==digest,name
 prefix=BASE/'deps/prefix';images=ROOT/'build/matrixgame/e2e519/windows-x64'
 image_source=ROOT/'build/matrixgame/e2e518/arm64-normalized'
 png=images/'png/Release/libpng16_static.lib';jpeg=images/'jpeg/Release/jpeg-static.lib';zlib=images/'zlib/Release/zs.lib'
 nro=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
 protected={str(p.relative_to(ROOT)):sha(p) for p in (png,jpeg,zlib,nro)}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
  log=p.stdout+p.stderr;logpath=BASE/(label+'.log');attempt=1
  while logpath.exists():logpath=BASE/f'{label}-{attempt}.log';attempt+=1
  logpath.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(logpath.relative_to(ROOT))})
  print(label,p.returncode,log[-6000:] if p.returncode or label=='runtime' else '',flush=True)
  assert not p.returncode,label
  return log
 try:
  source_root=BASE/'source';source_root.mkdir(exist_ok=True)
  upstream=ROOT/'upstream/cpp/src';old_overlay=ROOT/'port/switch/build/e2e-game-perf515/source-overlay'
  for tree in (upstream,old_overlay):
   for source in tree.rglob('*.hpp'):
    dest=source_root/source.relative_to(tree);dest.parent.mkdir(parents=True,exist_ok=True)
    if not dest.exists() or sha(source)!=sha(dest):shutil.copyfile(source,dest)
  runtime=BASE/'runtime-overlay';prepare_runtime_overlay(ROOT/'upstream/cpp/runtime',runtime,portable_windows_host=True)
  # The src include root is self-contained too, including runtime.hpp siblings.
  for source in runtime.glob('*.hpp'):shutil.copyfile(source,source_root/source.name)
  okgf_source=BASE/'okgf-source'
  if not okgf_source.exists():shutil.copytree(ROOT/'upstream/okgf',okgf_source,ignore=shutil.ignore_patterns('.git','build'))
  text=(ROOT/'upstream/okgf/CMakeLists.txt').read_text();assert text.count('add_library(okgf SHARED')==1
  expected=text.replace('add_library(okgf SHARED','add_library(okgf STATIC',1)
  if (okgf_source/'CMakeLists.txt').read_text()!=expected:(okgf_source/'CMakeLists.txt').write_text(expected)
  # SDK 10.0.26100 dxdiag.h's convenience macro incorrectly takes three
  # arguments; the actual COM interface takes only pParams after this.
  # Keep the real Initialize call, bypassing that malformed macro only.
  original=(ROOT/'upstream/okgf/src/platform_win32.c').read_text()
  old='IDxDiagProvider_Initialize(provider, &params)'
  assert original.count(old)==1
  rendered=original.replace(old,'provider->lpVtbl->Initialize(provider, &params)',1)
  target=okgf_source/'src/platform_win32.c'
  if target.read_text()!=rendered:target.write_text(rendered)
  result['okgf_windows_overlay']={'input_sha256':sha(ROOT/'upstream/okgf/src/platform_win32.c'),'output_sha256':sha(target),'change':'direct original COM vtable Initialize call; SDK macro arity defect'}
  run([CMAKE,'-S',okgf_source,'-B',BASE/'okgf','-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(NINJA),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DOKGF_MATH_BACKEND=NATIVE','-DPNG_PNG_INCLUDE_DIR='+str(image_source/'png-source'),'-DPNG_LIBRARY='+str(png),'-DJPEG_INCLUDE_DIR='+str(image_source/'jpeg-source'),'-DJPEG_LIBRARY='+str(jpeg),'-DZLIB_INCLUDE_DIR='+str(ROOT/'build/e2e515/deps/zlib-1.3.2'),'-DZLIB_LIBRARY='+str(zlib),'-DCMAKE_C_FLAGS=/experimental:c11atomics /I'+str(images/'jpeg')+' /I'+str(images/'png')],'configure-okgf')
  run([CMAKE,'--build',BASE/'okgf','-j','4'],'build-okgf')
  units=[];original_hashes={}
  report=read(ROOT/'port/switch/build/e2e5-host-game/switch-effective/report.json')
  for unit in report['compiled_units']:
   if unit['original']=='tools/e2e5_host_game_main.cpp':continue
   source=ROOT/unit['effective'].replace('\\','/')
   if source.name in ('EC_BlockPar.cpp','EC_Data.cpp'):source=ROOT/'build/matrixgame/e2e573-rangers-data/host-final/source'/source.name
   original_hashes[str(source.relative_to(ROOT))]=sha(source)
   category='original' if unit['original'].startswith('upstream/') else 'platform'
   if category=='original':
    dest=source_root/source.name
    if source.name=='runtime.cpp':source=runtime/'runtime.cpp'
    content=source.read_text(encoding='utf-8')
    # Preserve the effective port's required noinline boundary on MSVC.
    content=content.replace('__attribute__((noinline))','__declspec(noinline)')
    if not dest.exists() or dest.read_text(encoding='utf-8')!=content:dest.write_text(content,encoding='utf-8')
    source=dest
   units.append((source,BASE/'objects'/category/(Path(unit['original']).stem+'.obj')))
  assert len(units)==331
  units += [(ROOT/'port/matrixgame/native'/name,BASE/'objects/native'/(Path(name).stem+'.obj')) for name in ('rangers_bridge.cpp','engine_callbacks.cpp','rangers_host_callbacks.cpp','rangers_audio_session.cpp','rangers_music_session.cpp','rangers_data_session.cpp','rangers_host_session.cpp')]
  units += [(ROOT/'tests/probe_matrixgame_rangers_host_session.cpp',BASE/'objects/probe.obj')]
  includes=[source_root,runtime,ROOT/'port/switch/platform',ROOT/'upstream/okgf/include',prefix/'include',prefix/'include/SDL2',image_source/'png-source',images/'png',image_source/'jpeg-source',images/'jpeg',ROOT/'build/e2e515/deps/zlib-1.3.2',images/'zlib',ROOT/'port/matrixgame/native',ROOT/'port/matrixgame/native-core',ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/core/source']
  flags=['/nologo','/std:c++20','/EHsc','/utf-8','/MT','/O2','/UNDEBUG','/DNOMINMAX','/D_CRT_SECURE_NO_WARNINGS','/DSDL_MAIN_HANDLED']
  flags += ['/D'+flag[2:] for flag in report['compile_flags'] if flag.startswith('-D')]
  flags += ['/I'+str(p) for p in includes]
  result['flags']=flags;result['effective_inputs_sha256']=original_hashes
  result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p,_ in units}
  header_hashes={str(p.relative_to(ROOT)):sha(p) for root in includes if root.is_dir() for p in root.rglob('*.hpp')}
  result['headers_sha256']=header_hashes
  def compile_one(unit):
   source,obj=unit;obj.parent.mkdir(parents=True,exist_ok=True);stamp=obj.with_suffix('.stamp.json')
   expected={'source_sha256':sha(source),'flags':flags,'headers_sha256':header_hashes}
   if stamp.exists() and obj.exists():
    cached=read(stamp)
    if all(cached.get(k)==v for k,v in expected.items()) and cached.get('object_sha256')==sha(obj):return obj
   run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+obj.parent.name+'-'+obj.stem)
   stamp.write_text(json.dumps({**expected,'object_sha256':sha(obj)},indent=2)+'\n');return obj
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_one,units))
  result['objects_sha256']={str(p.relative_to(ROOT)):sha(p) for p in objects}
  libraries=[BASE/'okgf/okgf.lib',prefix/'lib/SDL2-static.lib',prefix/'lib/vorbisfile.lib',prefix/'lib/vorbis.lib',prefix/'lib/ogg.lib',png,jpeg,zlib]
  binary=BASE/'rangers-host-windows.exe';response=BASE/'link.rsp'
  response.write_text('\n'.join('"'+str(p)+'"' for p in objects+libraries),encoding='utf-8')
  run([compiler(env),'/nologo','/MT','@'+str(response),'/Fe'+str(binary),'/link','/INCREMENTAL:NO','oleaut32.lib','ole32.lib','user32.lib','gdi32.lib','version.lib','winmm.lib','setupapi.lib','imm32.lib','advapi32.lib','shell32.lib'],'link')
  private=BASE/'runtime';private.mkdir(exist_ok=True)
  result['stdout']=run([binary,GAME,private],'runtime')
  assert 'modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS' in result['stdout']
  for name,digest in {**original_hashes,**protected}.items():assert sha(ROOT/name)==digest,name
  result['protected_sha256']=protected;result['binary_sha256']=sha(binary)
  result['status']='PASS; MSVC WINDOWS LLP64 FOUR HOST AUDIO MODES; REAL SDL DUMMY ONLY'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

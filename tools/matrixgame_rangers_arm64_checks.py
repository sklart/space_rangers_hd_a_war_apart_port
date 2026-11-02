"""First Linux ARM64 execution of the effective original Rangers host chain.

All original effective units are compiled for AArch64, without replacing any
required services. Existing image libraries are read-only inputs; QEMU uses
the real SDL dummy backend and the normal glibc/pthread implementation.
"""
import concurrent.futures
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e576-rangers-arm64'
HOST=ROOT/'port/switch/build/e2e5-host-game/switch-effective'
GAME=ROOT/'windows/Space Rangers HD A War Apart'
def read(path):return json.loads(path.read_text())
def sha(path):
 with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def path(name):return ROOT/name.replace('\\','/')
def main():
 BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','ASan':'NOT RUN'}
 if (BASE/'result.json').exists():
  index=1
  while (BASE/f'result-attempt-{index}.json').exists():index+=1
  shutil.copyfile(BASE/'result.json',BASE/f'result-attempt-{index}.json')
 deps=read(BASE/'deps/result.json');assert deps['status'].startswith('PASS')
 for name,digest in {**deps['libraries_sha256'],**deps['headers_sha256']}.items():assert sha(path(name))==digest,name
 prefix=BASE/'deps/prefix';images=ROOT/'build/matrixgame/e2e518/arm64-normalized'
 png=images/'png/libpng16.a';jpeg=images/'jpeg/libjpeg.a';zlib=ROOT/'build/e2e515/mg-navigation-arm64/zlib/libz.a'
 protected={str(p.relative_to(ROOT)):sha(p) for p in (png,jpeg,zlib,ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro')}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True);log=p.stdout+p.stderr
  logpath=BASE/(label+'.log');attempt=1
  while logpath.exists():logpath=BASE/f'{label}-{attempt}.log';attempt+=1
  logpath.write_text(log);result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(logpath.relative_to(ROOT))});print(label,p.returncode,log[-7000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label
  return log
 try:
  result['compiler']=run(['aarch64-linux-gnu-g++','--version'],'compiler').splitlines()[0]
  result['qemu']=run(['qemu-aarch64','--version'],'qemu').splitlines()[0]
  # Preserve every C source; only the target type changes for a self-contained
  # cross-test link with the already built static PNG/JPEG dependencies.
  okgf_source=BASE/'okgf-source'
  if not okgf_source.exists():shutil.copytree(ROOT/'upstream/okgf',okgf_source,ignore=shutil.ignore_patterns('.git','build'))
  original=(ROOT/'upstream/okgf/CMakeLists.txt').read_text();assert original.count('add_library(okgf SHARED')==1
  expected=original.replace('add_library(okgf SHARED','add_library(okgf STATIC',1)
  if (okgf_source/'CMakeLists.txt').read_text()!=expected:(okgf_source/'CMakeLists.txt').write_text(expected)
  result['okgf_original_files_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'upstream/okgf').rglob('*') if p.is_file() and '.git' not in p.parts}
  run(['cmake','-S',okgf_source,'-B',BASE/'okgf','-DCMAKE_SYSTEM_NAME=Linux','-DCMAKE_SYSTEM_PROCESSOR=aarch64','-DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc','-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY','-DCMAKE_C_FLAGS=-I'+str(images/'jpeg'),'-DCMAKE_BUILD_TYPE=Release','-DOKGF_MATH_BACKEND=NATIVE','-DPNG_PNG_INCLUDE_DIR='+str(images/'png-source'),'-DPNG_LIBRARY='+str(png),'-DJPEG_INCLUDE_DIR='+str(images/'jpeg-source'),'-DJPEG_LIBRARY='+str(jpeg),'-DZLIB_INCLUDE_DIR='+str(ROOT/'build/e2e515/deps/zlib-1.3.2'),'-DZLIB_LIBRARY='+str(zlib)],'configure-okgf')
  run(['cmake','--build',BASE/'okgf','-j','4'],'build-okgf')
  report=read(HOST/'report.json');units=[]
  for unit in report['compiled_units']:
   if unit['original']=='tools/e2e5_host_game_main.cpp':continue
   source=path(unit['effective'])
   if source.name in ('EC_BlockPar.cpp','EC_Data.cpp'):source=ROOT/'build/matrixgame/e2e573-rangers-data/host-final/source'/source.name
   category='original' if unit['original'].startswith('upstream/') else 'platform'
   units.append((source,BASE/'objects'/category/(Path(unit['original']).stem+'.o')))
  assert len(units)==331
  units += [(ROOT/'port/matrixgame/native'/name,BASE/'objects/native'/(Path(name).stem+'.o')) for name in ('rangers_bridge.cpp','engine_callbacks.cpp','rangers_host_callbacks.cpp','rangers_audio_session.cpp','rangers_music_session.cpp','rangers_data_session.cpp','rangers_host_session.cpp')]
  units += [(ROOT/'tests/probe_matrixgame_rangers_host_session.cpp',BASE/'objects/probe.o')]
  flags=report['compile_flags']+['-UNDEBUG','-march=armv8-a+crc+crypto','-mtune=cortex-a57']
  flags=['-I'+str(prefix/'include'),'-I'+str(prefix/'include/SDL2'),'-I'+str(images/'png-source'),'-I'+str(images/'jpeg-source'),'-I'+str(images/'jpeg'),'-I'+str(ROOT/'build/e2e515/deps/zlib-1.3.2')]+flags
  flags += ['-I'+str(ROOT/'port/matrixgame/native'),'-I'+str(ROOT/'port/matrixgame/native-core'),'-I'+str(ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/core/source')]
  result['effective_sources_sha256']={str(p.relative_to(ROOT)):sha(p) for p,_ in units};result['flags']=flags
  def compile_one(unit):
   source,obj=unit;obj.parent.mkdir(parents=True,exist_ok=True);stamp=obj.with_suffix('.stamp.json');source_hash=sha(source)
   if stamp.exists() and obj.exists():
    cached=read(stamp)
    if cached['source_sha256']==source_hash and cached['flags']==flags and cached['object_sha256']==sha(obj):
     if all(Path(name).is_file() and sha(Path(name))==digest for name,digest in cached['headers_sha256'].items()):return obj
   run(['aarch64-linux-gnu-g++',*flags,'-MMD','-MF',obj.with_suffix('.d'),'-c',source,'-o',obj],'compile-'+obj.parent.name+'-'+obj.stem)
   text=obj.with_suffix('.d').read_text().replace('\\\n',' ');_,text=text.split(':',1)
   headers=[Path(s.replace('\\ ', ' ')) for s in re.split(r'(?<!\\)\s+',text.strip()) if s]
   stamp.write_text(json.dumps({'source_sha256':source_hash,'flags':flags,'object_sha256':sha(obj),'headers_sha256':{str(p):sha(p) for p in headers}},indent=2)+'\n');return obj
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
   objects=list(pool.map(compile_one,units))
  assert len(objects)==339
  result['objects_sha256']={str(p.relative_to(ROOT)):sha(p) for p in objects}
  stage=Path(tempfile.mkdtemp(prefix='matrixgame-rangers-arm64-link-'));local=[]
  for index,obj in enumerate(objects):dest=stage/f'{index:03d}.o';shutil.copyfile(obj,dest);local.append(dest)
  libraries=[BASE/'okgf/libokgf.a',prefix/'lib/libSDL2.a',prefix/'lib/libvorbisfile.a',prefix/'lib/libvorbis.a',prefix/'lib/libogg.a',png,jpeg,zlib]
  staged=[]
  for index,lib in enumerate(libraries):dest=stage/f'lib{index:02d}.a';shutil.copyfile(lib,dest);staged.append(dest)
  response=stage/'objects.rsp';response.write_text('\n'.join(map(str,local))+'\n');binary=BASE/'rangers-host-arm64'
  run(['aarch64-linux-gnu-g++','@'+str(response),'-Wl,--gc-sections','-Wl,--start-group',*staged,'-Wl,--end-group','-lm','-ldl','-pthread','-o',stage/'rangers-host-arm64'],'link')
  shutil.copyfile(stage/'rangers-host-arm64',binary);binary.chmod(0o755);result['staging']=str(stage)
  result['elf_header']=run(['aarch64-linux-gnu-readelf','-h',binary],'elf-header');assert 'AArch64' in result['elf_header']
  runtime=BASE/'runtime';runtime.mkdir(exist_ok=True)
  result['stdout']=run(['env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','qemu-aarch64','-L','/usr/aarch64-linux-gnu',binary,GAME,runtime],'runtime')
  assert 'modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS' in result['stdout']
  for name,digest in {**result['effective_sources_sha256'],**protected}.items():assert sha(path(name))==digest,name
  result['libraries_sha256']={str(p.relative_to(ROOT)):sha(p) for p in libraries};result['protected_sha256']=protected;result['binary_sha256']=sha(binary)
  result['status']='PASS; ARM64/QEMU REAL RANGERS HOST FOUR AUDIO MODES; SDL DUMMY ONLY'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()

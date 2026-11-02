"""ARM64 original AFT pixel differential and real hosted native Init rollback.

Reuses the first effective ARM64 object build, without rebuilding the big corpus.
"""
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e576-rangers-arm64'
OUT=BASE/'extra'
def read(p):return json.loads(p.read_text())
def sha(p):
 with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def path(name):
 p=Path(name.replace('\\','/').replace('D:/','/mnt/d/'));return p if p.is_absolute() else ROOT/p
def check(values):
 for name,digest in values.items():assert sha(path(name))==digest,name
def main():
 OUT.mkdir(parents=True,exist_ok=True);core=read(BASE/'result.json');assert core['status'].startswith('PASS');check(core['objects_sha256']);check(core['libraries_sha256'])
 native=read(ROOT/'build/matrixgame/e2e568-native-services/arm64/result.json')
 # Its old fixture TU is excluded from this link; verify the reused production
 # units/headers, then compile our current real-host fixture separately.
 native_sources={name:value for name,value in native['source_sha256'].items() if not name.replace('\\','/').endswith('/test_matrixgame_native_services.cpp')}
 check(native_sources);check(native['archive_sha256'])
 game_inputs=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/host/result.json')['protected_game_sha256'];check(game_inputs)
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','ASan':'NOT RUN'}
 sources=[ROOT/'tests/probe_matrixgame_rangers_text.cpp',ROOT/'tests/probe_matrixgame_hosted_native_prefix.cpp']
 before={str(p.relative_to(ROOT)):sha(p) for p in sources+[Path(__file__).resolve()]}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True);log=p.stdout+p.stderr
  (OUT/(label+'.log')).write_text(log);result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-7000:] if p.returncode or label.startswith('runtime') else '',flush=True);assert not p.returncode,label;return log
 def link(objects,archives,binary):
  stage=Path(tempfile.mkdtemp(prefix='matrixgame-arm64-extra-'));local=[]
  for index,obj in enumerate(objects):dest=stage/f'{index:03d}.o';shutil.copyfile(obj,dest);local.append(dest)
  libs=[]
  for index,archive in enumerate([*archives,*map(path,core['libraries_sha256'])]):dest=stage/f'lib{index:02d}.a';shutil.copyfile(archive,dest);libs.append(dest)
  response=stage/'objects.rsp';response.write_text('\n'.join(map(str,local))+'\n')
  run(['aarch64-linux-gnu-g++','@'+str(response),'-Wl,--gc-sections','-no-pie','-Wl,--start-group',*libs,'-Wl,--end-group','-lm','-ldl','-pthread','-o',stage/binary],'link-'+binary)
  shutil.copyfile(stage/binary,OUT/binary);(OUT/binary).chmod(0o755)
 def runtime(binary,dest,extra=()):
  dest.mkdir(parents=True,exist_ok=True)
  return run(['env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','qemu-aarch64','-L','/usr/aarch64-linux-gnu',OUT/binary,ROOT/'windows/Space Rangers HD A War Apart',dest,*extra],'runtime-'+binary)
 try:
  flags=core['flags'];text_obj=OUT/'text.o';run(['aarch64-linux-gnu-g++',*flags,'-c',sources[0],'-o',text_obj],'compile-text')
  objects={name:value for name,value in core['objects_sha256'].items() if not name.endswith('/probe.o')}
  link([text_obj,*map(path,objects)],[],'rangers-text')
  result['text_stdout']=runtime('rangers-text',OUT/'text-runtime')
  assert 'exact_original_pairs=24 crop=1 automatic=1 error_cleanup=PASS' in result['text_stdout']
  pixels=OUT/'text-runtime/pixels.bin';reference=ROOT/'build/matrixgame/e2e570-rangers-text/host/runtime/pixels.bin'
  assert pixels.read_bytes()==reference.read_bytes() and pixels.stat().st_size==24*200*64*4
  command=next(c['command'] for c in native['commands'] if c['label']=='compile-init_runtime')[2:]
  prefix_obj=OUT/'native-prefix.o';command[command.index('-c')+1]=str(sources[1]);command[command.index('-o')+1]=str(prefix_obj)
  command += ['-I'+str(ROOT/'port/matrixgame/native'),'-I'+str(ROOT/'upstream/cpp/src'),'-I'+str(ROOT/'upstream/cpp/runtime'),'-I'+str(ROOT/'port/switch/platform'),'-I'+str(BASE/'deps/prefix/include')]
  run(command,'compile-native-prefix')
  native_link=next(c['command'] for c in native['commands'] if c['label']=='link')
  native_objects=[path(name) for name in native_link if name.endswith('.o') and not name.endswith('test_matrixgame_native_services.o')]
  combined=[path(name) for name in objects if not name.endswith('/package.o')]
  native_archives=list(map(path,native['archive_sha256']))
  link([prefix_obj,*combined,*native_objects],native_archives,'hosted-native-prefix')
  native_root=ROOT/'build/matrixgame/e2e574-rangers-host-session/native-prefix-final/native-input'
  fixture={str(p.relative_to(ROOT)):sha(p) for p in (native_root/'CFG/robots/data.txt',native_root/'CFG/robots/iface.txt',ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')}
  result['prefix_stdout']=runtime('hosted-native-prefix',OUT/'prefix-runtime',[native_root])
  assert 'original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS' in result['prefix_stdout']
  check(before);check(objects);check(native['archive_sha256']);check(fixture);check(game_inputs)
  result['reused_objects_sha256']=objects;result['native_objects_sha256']={str(p.relative_to(ROOT)):sha(p) for p in native_objects}
  result['native_archives_sha256']=native['archive_sha256'];result['native_fixture_sha256']=fixture
  result['native_sources_sha256']=native_sources
  result['protected_game_sha256']=game_inputs
  result['new_objects_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (text_obj,prefix_obj)}
  result['binaries']={name:sha(OUT/name) for name in ('rangers-text','hosted-native-prefix')};result['pixel_sha256']=sha(pixels);result['reference_pixel_sha256']=sha(reference)
  result['status']='PASS; ARM64/QEMU REAL AFT PIXELS + HOSTED ORIGINAL INIT CAPS ROLLBACK; NO GPU'
 finally:
  result['source_sha256']=before;(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()

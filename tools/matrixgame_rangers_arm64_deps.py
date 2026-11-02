"""Linux-hosted, workspace-only ARM64 SDL dummy/Ogg/Vorbis test dependencies.

Pinned upstream source archives; no system install, SDK edits or Switch access.
"""
import hashlib
import json
import subprocess
import tarfile
import urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e576-rangers-arm64/deps'
SOURCES=[
 ('ogg','libogg-1.3.6','https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz','5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061'),
 ('vorbis','libvorbis-1.3.7','https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz','b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b'),
 ('sdl','SDL2-2.32.10','https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz','5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165')]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 BASE.mkdir(parents=True,exist_ok=True);prefix=BASE/'prefix';result={'status':'INCOMPLETE','commands':[],'archives':[],'Switch':'NOT RUN'}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True);log=p.stdout+p.stderr
  (BASE/(label+'.log')).write_text(log);result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-7000:] if p.returncode else '',flush=True);assert not p.returncode,label
 try:
  for name,folder,url,digest in SOURCES:
   archive=BASE/url.rsplit('/',1)[1]
   if not archive.exists():
    with urllib.request.urlopen(url,timeout=60) as response:archive.write_bytes(response.read())
   assert sha(archive)==digest,archive
   result['archives'].append({'name':name,'url':url,'sha256':digest,'path':str(archive.relative_to(ROOT))})
   source=BASE/'source'/folder;stamp=BASE/'source'/(folder+'.extracted.json')
   if not stamp.exists():
    source.parent.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive) as package:package.extractall(source.parent,filter='data')
    assert source.is_dir(),source
    stamp.write_text(json.dumps({'archive_sha256':digest})+'\n')
   else:assert json.loads(stamp.read_text())['archive_sha256']==digest
   command=['cmake','-S',source,'-B',BASE/name,'-DCMAKE_SYSTEM_NAME=Linux','-DCMAKE_SYSTEM_PROCESSOR=aarch64','-DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc','-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++','-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_POSITION_INDEPENDENT_CODE=ON','-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF']
   if name=='vorbis':command+=['-DOGG_INCLUDE_DIR='+str(prefix/'include'),'-DOGG_LIBRARY='+str(prefix/'lib/libogg.a')]
   if name=='sdl':
    command += ['-DSDL_SHARED=OFF','-DSDL_STATIC=ON','-DSDL_TEST=OFF','-DSDL_TESTS=OFF','-DSDL_AUDIO=ON','-DSDL_DUMMYAUDIO=ON','-DSDL_DUMMYVIDEO=ON']
    command += ['-DSDL_'+feature+'=OFF' for feature in ('X11','WAYLAND','KMSDRM','OPENGL','OPENGLES','VULKAN','ALSA','PULSEAUDIO','PIPEWIRE','JACK','SNDIO','ESD','ARTS','NAS','DISKAUDIO','OSS','LIBSAMPLERATE','HIDAPI','UDEV','DBUS','IBUS','FCITX')]
   run(command,'configure-'+name);run(['cmake','--build',BASE/name,'-j','4'],'build-'+name);run(['cmake','--install',BASE/name],'install-'+name)
  paths=[prefix/'lib'/name for name in ('libSDL2.a','libogg.a','libvorbis.a','libvorbisfile.a')]
  result['libraries_sha256']={str(path.relative_to(ROOT)):sha(path) for path in paths}
  result['headers_sha256']={str(path.relative_to(ROOT)):sha(path) for path in (prefix/'include').rglob('*') if path.is_file()}
  result['status']='PASS; ARM64 STATIC DEPENDENCIES; SDL REAL DUMMY BACKEND ONLY; NO SYSTEM INSTALL'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()

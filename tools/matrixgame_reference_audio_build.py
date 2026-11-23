"""Build Ogg/Vorbis for the isolated MSVC reference environment."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment,compiler,CMAKE,NINJA
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def main():
 output=BASE/'audio-libs'
 if output.exists():raise RuntimeError('audio build already exists')
 output.mkdir();env=environment(output);result={'status':'INCOMPLETE','commands':[]};prefix=output/'prefix'
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (output/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 base=[CMAKE,'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(NINJA),'-DCMAKE_BUILD_TYPE=Release',
       '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DCMAKE_C_COMPILER='+str(compiler(env)),
       '-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF','-DINSTALL_DOCS=OFF','-DCMAKE_INSTALL_PREFIX='+str(prefix)]
 try:
  for name in ('ogg','vorbis'):
   src=BASE/('audio-sources/libogg-1.3.5' if name=='ogg' else 'audio-sources/libvorbis-1.3.7');build=output/name
   run(name+'-configure',[*base,'-S',src,'-B',build,'-DCMAKE_PREFIX_PATH='+str(prefix)])
   run(name+'-build',[CMAKE,'--build',build,'--target','install','-j','4'])
  libraries=list(prefix.rglob('*.lib'));assert len(libraries)>=3
  result['libraries']={str(p.relative_to(ROOT)):hashlib.file_digest(p.open('rb'),'sha256').hexdigest() for p in libraries}
  result['status']='PASS; MSVC x64 Ogg/Vorbis libraries; playback NOT RUN'
 finally:(output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

"""Build fresh MSVC x64 image libraries in the isolated reference workspace."""
import json,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment,compiler,CMAKE,NINJA
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'

def main():
 out=BASE/'image-libs'
 if out.exists():raise RuntimeError('image build already exists')
 out.mkdir();env=environment(out)
 result={'status':'INCOMPLETE','commands':[]}
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:print((p.stdout+p.stderr)[-5000:],flush=True);raise RuntimeError(label)
 base=[CMAKE,'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(NINJA),'-DCMAKE_BUILD_TYPE=Release',
       '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DCMAKE_C_COMPILER='+str(compiler(env)),
       '-DCMAKE_CXX_COMPILER='+str(compiler(env)),'-DBUILD_TESTING=OFF']
 try:
  zsrc=BASE/'source-deps-retry/zlib-1.3.2';zbuild=out/'zlib'
  run('zlib-configure',[*base,'-S',zsrc,'-B',zbuild,'-DZLIB_BUILD_TESTING=OFF','-DZLIB_BUILD_SHARED=OFF','-DZLIB_BUILD_STATIC=ON'])
  run('zlib-build',[CMAKE,'--build',zbuild,'--config','Release','-j','4'])
  libraries=list(zbuild.rglob('*.lib'));static=[p for p in libraries if 'static' in p.name.lower() or p.name.lower() in ('zlib.lib','zlibstatic.lib','zs.lib')]
  if len(static)!=1:raise RuntimeError('unexpected zlib libraries: '+str(libraries))
  zlib=static[0]
  psrc=BASE/'source-deps-retry/libpng-1.6.48';pbuild=out/'png'
  run('png-configure',[*base,'-S',psrc,'-B',pbuild,'-DPNG_SHARED=OFF','-DPNG_STATIC=ON','-DPNG_TESTS=OFF',
       '-DZLIB_INCLUDE_DIR='+str(zsrc),'-DZLIB_LIBRARY='+str(zlib)])
  run('png-build',[CMAKE,'--build',pbuild,'--config','Release','-j','4'])
  jsrc=BASE/'source-deps/libjpeg-turbo-3.1.0';jbuild=out/'jpeg'
  run('jpeg-configure',[*base,'-S',jsrc,'-B',jbuild,'-DENABLE_SHARED=OFF','-DENABLE_STATIC=ON','-DWITH_SIMD=OFF','-DWITH_TURBOJPEG=OFF'])
  run('jpeg-build',[CMAKE,'--build',jbuild,'--config','Release','-j','4'])
  result['libraries']=[str(p) for p in out.rglob('*.lib')];result['status']='PASS; MSVC image libraries built; not game runtime'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

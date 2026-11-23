"""Rebuild Windows MatrixGame archives using isolated, newly built dependencies."""
import hashlib,json,subprocess,sys
from pathlib import Path
from matrixgame_windows_host_env import environment,compiler,CMAKE,NINJA
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 output=BASE/'native-windows-run'
 if output.exists():raise RuntimeError('native reference build already exists')
 output.mkdir();env=environment(output);result={'status':'INCOMPLETE','commands':[]}
 images=BASE/'image-libs';sources=BASE/'source-deps-retry';jpeg=BASE/'source-deps/libjpeg-turbo-3.1.0/src'
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (output/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:print((p.stdout+p.stderr)[-6000:],flush=True);raise RuntimeError(label)
 try:
  png=images/'png/libpng16_static.lib';jpg=images/'jpeg/jpeg-static.lib';zlib=images/'zlib/zs.lib'
  assert all(p.is_file() for p in (png,jpg,zlib))
  flags='/EHsc /utf-8 /UNDEBUG /DNOMINMAX /I'+str(images/'png')+' /I'+str(images/'jpeg')+' /I'+str(images/'zlib')
  run('configure',[CMAKE,'-S',ROOT/'port/matrixgame/native-engine','-B',output,'-G','Ninja',
      '-DCMAKE_MAKE_PROGRAM='+str(NINJA),'-DCMAKE_CXX_COMPILER='+str(compiler(env)),
      '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',
      '-DPython3_EXECUTABLE='+sys.executable,'-DBUILD_TESTING=OFF','-DCORE_BUILD_LINK_PROBE=OFF',
      '-DCORE_SANITIZERS=OFF','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS='+flags,
      '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2','-DMATRIXGAME_BUILD_RANGERS_RUN=ON','-DPNG_PNG_INCLUDE_DIR='+str(sources/'libpng-1.6.48'),'-DPNG_LIBRARY='+str(png),
      '-DJPEG_INCLUDE_DIR='+str(jpeg),'-DJPEG_LIBRARY='+str(jpg),'-DZLIB_INCLUDE_DIR='+str(sources/'zlib-1.3.2'),
      '-DZLIB_LIBRARY='+str(zlib),'-DMATRIXGAME_D3DX9_ROOT='+str(BASE/'deps/d3dx')])
  run('build',[CMAKE,'--build',output,'--target','matrixgame-native-rangers-interface','-j','4'])
  archives=list(output.rglob('*.lib'));assert len(archives)>=4
  result['archives']={str(p.relative_to(ROOT)):sha(p) for p in archives}
  result['status']='PASS; MSVC x64 MatrixGame and Rangers interface archives; runtime NOT RUN'
 finally:(output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

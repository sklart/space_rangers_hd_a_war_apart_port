"""Build original OKGF as an isolated Windows x64 static library."""
import hashlib,json,shutil,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment,compiler,CMAKE,NINJA
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def main():
 output=BASE/'okgf-windows';source=BASE/'okgf-source'
 if output.exists() or source.exists():raise RuntimeError('OKGF build already exists')
 shutil.copytree(ROOT/'upstream/okgf',source,ignore=shutil.ignore_patterns('.git','build'));output.mkdir();env=environment(output)
 result={'status':'INCOMPLETE','commands':[]}
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (output/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 try:
  cmake=source/'CMakeLists.txt';text=cmake.read_text(encoding='utf8');assert text.count('add_library(okgf SHARED')==1
  cmake.write_text(text.replace('add_library(okgf SHARED','add_library(okgf STATIC',1),encoding='utf8')
  platform=source/'src/platform_win32.c';text=platform.read_text(encoding='utf8');before='IDxDiagProvider_Initialize(provider, &params)';assert text.count(before)==1
  platform.write_text(text.replace(before,'provider->lpVtbl->Initialize(provider, &params)',1),encoding='utf8')
  img=BASE/'image-libs';src=BASE/'source-deps-retry';jpeg=BASE/'source-deps/libjpeg-turbo-3.1.0/src'
  flags='/experimental:c11atomics /I'+str(img/'jpeg')+' /I'+str(img/'png')+' /I'+str(img/'zlib')
  run('configure',[CMAKE,'-S',source,'-B',output,'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(NINJA),
      '-DCMAKE_C_COMPILER='+str(compiler(env)),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',
      '-DOKGF_MATH_BACKEND=NATIVE','-DPNG_PNG_INCLUDE_DIR='+str(src/'libpng-1.6.48'),
      '-DPNG_LIBRARY='+str(img/'png/libpng16_static.lib'),'-DJPEG_INCLUDE_DIR='+str(jpeg),
      '-DJPEG_LIBRARY='+str(img/'jpeg/jpeg-static.lib'),'-DZLIB_INCLUDE_DIR='+str(src/'zlib-1.3.2'),
      '-DZLIB_LIBRARY='+str(img/'zlib/zs.lib'),'-DCMAKE_C_FLAGS='+flags])
  run('build',[CMAKE,'--build',output,'-j','4'])
  library=output/'okgf.lib';assert library.is_file()
  with library.open('rb') as data:result['sha256']=hashlib.file_digest(data,'sha256').hexdigest()
  result['status']='PASS; original OKGF MSVC static library; runtime NOT RUN'
 finally:(output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()

"""Actual HAL/production GLSL line comparison and addressed native state checks."""
import argparse,hashlib,json,os,struct,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e537-lines'
sys.stdout.reconfigure(encoding='utf-8')
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 global BASE
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['windows','host','arm64']);ap.add_argument('--output',type=Path,default=BASE);ap.add_argument('--actual-only',action='store_true');args=ap.parse_args();kind=args.kind;BASE=args.output.resolve()
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 paths=['port/matrixgame/native-engine/'+n for n in ('native_device_state.cpp','native_device_state.hpp','native_state_layout.hpp','platform_switch.cpp')]+['port/matrixgame/deko/'+n for n in ('fixed_vertex.cpp','material_combiner.cpp','material.vert.glsl','material.frag.glsl','legacy_raster_viewport.hpp')]
 paths+=['tests/'+n for n in ('matrixgame_line_windows.cpp','matrixgame_fog_windows.cpp','matrixgame_material_gl_reference.hpp','matrixgame_actual_draw_state.hpp','test_matrixgame_line_state.cpp')]
 def run(cmd,label):
  if isinstance(cmd,str):
   script=out/(label+'.cmd');script.write_text(cmd+'\n');cmd=['cmd.exe','/d','/c',script]
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');stdout=p.stdout+p.stderr
  (out/(label+'.log')).write_text(stdout,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start})
  print(label,p.returncode,stdout[-4000:] if p.returncode or label=='runtime' else 'saved',flush=True)
  if label=='runtime':j['stdout']=stdout
  if p.returncode:j['status']='FAIL';raise RuntimeError(label+' failed')
  return stdout
 try:
  if kind=='windows':
   template=(ROOT/'build/matrixgame/e2e533-form/windows/build-platform_win32.cmd').read_text().split(' /c ')[0]+' /I"'+str(ROOT/'port/matrixgame/deko')+'"'
   sources=[ROOT/p for p in ('tests/matrixgame_line_windows.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp')]
   objects=[]
   for source in sources:
    obj=out/(source.stem+'.obj');run(template+' /I"'+str(ROOT/'tests')+'" /utf-8 /c "'+str(source)+'" /Fo"'+str(obj)+'"','build-'+source.stem);objects.append(obj)
   vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
   run('call "'+vs+'" -no_logo -arch=x86 -host_arch=x64 && link /NOLOGO /OPT:REF /OUT:"'+str(out/'line-oracle.exe')+'" '+' '.join('"'+str(o)+'"' for o in objects)+' d3d9.lib user32.lib gdi32.lib opengl32.lib','link')
   captures=sorted((BASE/'map-state/windows').glob('line-geometry-*.bin'));assert len(captures)==2
   j['capture_sha256']={str(p.relative_to(ROOT)):sha(p) for p in captures}
   expected_cases=expected_pixels=0
   for path in captures:
    data=path.read_bytes();magic,version,size,count=struct.unpack_from('<4I',data);assert (magic,version,size)==(0x374c524d,1,2808);offset=16
    for _ in range(count):
     offset+=size;primitives,stride,width,height,fmt,byte_count=struct.unpack_from('<6I',data,offset);offset+=24+byte_count;expected_cases+=1;expected_pixels+=width*height
    assert offset==len(data)
   j['binary_sha256']=sha(out/'line-oracle.exe')
   run([out/'line-oracle.exe',ROOT/'port/matrixgame/deko/material.frag.glsl',*(['--actual-only'] if args.actual_only else []),*captures],'runtime')
   if not args.actual_only:assert 'last0_mismatches=0 last1_mismatches=0 max_delta=0' in j['stdout']
   assert f'ACTUAL_ORIGINAL_LINE_HAL_GLSL cases={expected_cases} pixels={expected_pixels} mismatches=0 max_delta=0' in j['stdout']
   assert 'VIEWPORT_TRIANGLE_CLIP_HAL_GLSL cases=24 mismatches=0' in j['stdout']
   j.update(actual_cases=expected_cases,actual_pixels=expected_pixels,synthetic_suite_repeated=not args.actual_only)
  else:
   sources=[ROOT/p for p in ('tests/test_matrixgame_line_state.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp')]
   original=ROOT/'build/matrixgame/e2e536-actual-draw/sdk/source';includes=[ROOT/p for p in ('tests','port/matrixgame/native-engine','port/matrixgame/native-core','port/matrixgame/deko')]+[original/'portable-core',original/'MatrixLib/Base']
   flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-pie','-no-pie','-pthread']
   if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
   exe=out/'line-state';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*map(linux,sources),'-o',linux(exe)],'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   run(cmd+[linux(exe)],'runtime');j['binary_sha256']=sha(exe)
  j['status']='PASS'
 finally:
  j['source_sha256']={p:sha(ROOT/p) for p in paths}
  (out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

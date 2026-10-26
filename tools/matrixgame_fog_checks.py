"""Native pixel fog: real HAL/GLSL oracle, addressed CPU/QEMU and SDK ELF."""
import argparse,hashlib,json,os,subprocess,sys,time
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e530-fog'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 global BASE
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['windows','host','arm64','sdk']);ap.add_argument('--output',type=Path,default=BASE);args=ap.parse_args();BASE=args.output.resolve();kind=args.kind;out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'HARDWARE_PENDING'}
 paths=['port/matrixgame/deko/'+n for n in ('pixel_fog.hpp','material.frag.glsl','material.vert.glsl','material_program.cpp','material_program.hpp','fixed_vertex.cpp','fixed_vertex.hpp','material_combiner.cpp','material_combiner.hpp')]+['port/matrixgame/native-engine/'+n for n in ('native_device_state.cpp','native_device_state.hpp','native_map_queries.hpp','map_device_backend.cpp','platform_switch.cpp','platform_switch.hpp')]+['tests/matrixgame_fog_windows.cpp','tests/matrixgame_material_gl_reference.hpp','tools/matrixgame_fog_checks.py']
 paths+=['port/matrixgame/deko/legacy_raster_viewport.hpp','tests/test_matrixgame_pixel_fog.cpp']
 paths+=['port/matrixgame/deko/stencil_state.hpp','port/matrixgame/deko/stencil_deko.hpp']
 original=ROOT/'build/matrixgame/e2e529-stored-state/sdk/source'
 sources=['tests/test_matrixgame_pixel_fog.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_device_state.cpp','native_stored_state.cpp','map_device_backend.cpp','platform_3g.cpp')]+['port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp',str((original/'MatrixLib/3G/DeviceState.cpp').relative_to(ROOT))]
 if kind in ('host','arm64'):paths+=sources+['port/matrixgame/native-engine/native_stored_state.hpp','port/matrixgame/native-engine/native_state_layout.hpp']
 before={n:sha(ROOT/n) for n in paths}
 def run(cmd,label,env=None):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-3000:] if label!='symbols' else 'saved',flush=True);assert p.returncode==0;return p.stdout
 try:
  if kind=='windows':
   vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
   command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /utf-8 /MD /EHsc /std:c++20 /O2 /UNDEBUG '
   command+=' '.join('/I"'+str(ROOT/p)+'"' for p in ('tests','port/matrixgame/deko','port/matrixgame/native-engine','port/matrixgame/native-core'))+' '
   command+=' '.join('"'+str(ROOT/p)+'"' for p in ('tests/matrixgame_fog_windows.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp'))
   command+=' /Fo"'+out.as_posix()+'/" /Fe"'+str(out/'fog-oracle.exe')+'" /link d3d9.lib user32.lib gdi32.lib opengl32.lib'
   script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');j['stdout']=run([out/'fog-oracle.exe',ROOT/'port/matrixgame/deko/material.frag.glsl',out/'fog.golden'],'runtime');j['golden_sha256']=sha(out/'fog.golden')
  elif kind=='sdk':
   cmd=json.loads((ROOT/'build/matrixgame/e2e529-stored-state/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');j['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not j['strong_undefined']
   for name in ('DekoDevice::NativeRasterCaps()','NativeRenderState::PixelFog()','DekoCombinerProgram::Prepare(', 'CGame::Init('):assert name in nm,name
   j['elf_sha256']=sha(elf);j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  else:
   flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffp-contract=off','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-pie','-no-pie','-pthread']
   if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
   includes=[ROOT/p for p in ('port/matrixgame/deko','port/matrixgame/native-engine','port/matrixgame/native-core')]+[original/p for p in ('portable-core','MatrixLib/3G','MatrixLib/Base')]
   target=out/'pixel-fog';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in sources],'-o',linux(target)],'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   j['stdout']=run(cmd+[linux(target),linux(BASE/'windows/fog.golden')],'runtime');j['golden_sha256']=sha(BASE/'windows/fog.golden');j['binary_sha256']=sha(target)
  after={n:sha(ROOT/n) for n in paths};assert before==after,'input changed during checks';j['source_sha256']=after;j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

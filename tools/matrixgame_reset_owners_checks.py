"""Native reset resource ownership: real HAL oracle, CPU baseline/candidate, SDK."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e534-reset-owners'
sys.stdout.reconfigure(encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['windows','baseline','host','arm64','sdk','regression-host']);kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 def run(cmd,label,env=None):
  p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-5000:] if p.returncode or label=='runtime' else 'saved',flush=True)
  if label=='runtime':j['stdout']=p.stdout+p.stderr
  if p.returncode:j['status']='FAIL';raise RuntimeError(label+' failed')
  return p.stdout
 try:
  if kind=='windows':
   paths=['tests/matrixgame_reset_owners_windows.cpp'];source=ROOT/paths[0];vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
   script=out/'build.cmd';script.write_text(f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /std:c++20 /MD /EHsc /O2 /UNDEBUG "{source}" /Fo"{out}/" /Fe"{out}/oracle.exe" /link d3d9.lib user32.lib\n')
   j['source_sha256']={p:sha(ROOT/p) for p in paths}
   run(['cmd.exe','/d','/c',script],'build');j['binary_sha256']=sha(out/'oracle.exe');j['stdout']=run([out/'oracle.exe'],'runtime')
  elif kind=='regression-host':
   paths=['tests/test_matrixgame_native_surfaces.cpp','tests/test_matrixgame_render_textures.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_surfaces.hpp','native_surfaces.cpp','native_texture.hpp','native_texture.cpp')]
   j['profiles']={}
   for label,prior in [('surfaces','e2e525-surfaces/host'),('render-textures','e2e526-render-textures/render-host')]:
    cmd=json.loads((ROOT/'build/matrixgame'/prior/'result.json').read_text())['commands'][0]['command'][:];exe=out/label;cmd[cmd.index('-o')+1]=linux(exe)
    assert '-fsanitize=address,undefined' in cmd
    run(cmd,label+'-build');stdout=run(['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',linux(exe)],label+'-runtime')
    j['profiles'][label]={'stdout':stdout,'binary_sha256':sha(exe)}
  elif kind=='baseline':
   original=ROOT/'build/matrixgame/e2e533-form/sdk/source'
   paths=['tests/test_matrixgame_reset_owners_baseline.cpp','port/matrixgame/native-core/buffer_resources.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_device_state.cpp','native_texture.cpp')]+['port/matrixgame/deko/'+n for n in ('fixed_vertex.cpp','material_combiner.cpp')]
   includes=[ROOT/p for p in ('port/matrixgame','port/matrixgame/native-engine','port/matrixgame/native-core','port/matrixgame/deko')]+[original/'portable-core',original/'MatrixLib/Base']
   exe=out/'baseline';run(['wsl','--exec','g++','-std=c++20','-O1','-g','-UNDEBUG','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-pthread',*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in paths],'-o',linux(exe)],'build');j['stdout']=run(['wsl','--exec',linux(exe)],'runtime');j['binary_sha256']=sha(exe)
  elif kind in ('host','arm64'):
   original=ROOT/'build/matrixgame/e2e533-form/sdk/source'
   paths=['tests/test_matrixgame_reset_owners.cpp','port/matrixgame/native-core/buffer_resources.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_device_state.cpp','native_texture.cpp','native_surfaces.cpp')]+['port/matrixgame/deko/'+n for n in ('fixed_vertex.cpp','material_combiner.cpp')]
   includes=[ROOT/p for p in ('port/matrixgame','port/matrixgame/native-engine','port/matrixgame/native-core','port/matrixgame/deko')]+[original/'portable-core',original/'MatrixLib/Base']
   flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-pthread','-fno-pie','-no-pie']
   if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
   exe=out/'reset-owners';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in paths],'-o',linux(exe)],'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   j['stdout']=run(cmd+[linux(exe)],'runtime');j['binary_sha256']=sha(exe)
   paths+=['port/matrixgame/native-engine/'+n for n in ('native_reset_resources.hpp','native_texture.hpp','native_surfaces.hpp','native_device_state.hpp')]
  else:
   paths=['port/matrixgame/native-engine/'+n for n in ('native_reset_resources.hpp','native_texture.hpp','native_surfaces.hpp','native_device_state.hpp','native_surfaces.cpp','native_device_state.cpp','platform_switch.cpp')]
   cmd=json.loads((ROOT/'build/matrixgame/e2e533-form/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
   run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');j['strong_undefined']=[line for line in nm.splitlines() if ' U ' in line];assert not j['strong_undefined']
   for name in ('CGame::Init(', 'RunOriginalNativeLoop()', 'NativeStateStore::AddDeviceTextureReferences(', 'native_surfaces::Session::ExternalDefaultResourcesLive()', 'ResetTextureReference() const'):assert name in nm,name
   ninja=next(c.split('=',1)[1] for c in cmd if c.startswith('-DCMAKE_MAKE_PROGRAM='));graph=run([ninja,'-C',out,'-t','graph','matrixgame-native-entry.elf'],'link-graph');inputs=set()
   for label in re.findall(r'\[label="((?:\\.|[^"\\])*)"',graph):
    p=(out/Path(json.loads('"'+label+'"'))).resolve()
    if p.is_relative_to(ROOT) and p.suffix in ('.o','.obj','.a') and p.is_file():inputs.add(p)
   j['elf_sha256']=sha(elf);j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sorted(inputs)}
  if kind=='baseline':
   paths+=['port/matrixgame/native-engine/'+n for n in ('native_texture.hpp','native_device_state.hpp','native_surfaces.hpp','native_surfaces.cpp','platform_switch.cpp')]
   for name in paths:
    copy=out/'source'/name;copy.parent.mkdir(parents=True,exist_ok=True);copy.write_bytes((ROOT/name).read_bytes())
  j['source_sha256']={p:sha(ROOT/p) for p in paths};j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

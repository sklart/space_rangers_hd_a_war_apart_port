"""Addressed native surface ownership and full devkitA64 ELF. No NRO/hardware."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e525-surfaces'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['sdk','host','arm64','solid-host','solid-arm64']);kind=ap.parse_args().kind;out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 def run(cmd,label,env=None):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-2400:] if label!='symbols' else 'saved',flush=True);assert p.returncode==0;return p.stdout
 try:
  if kind=='sdk':
   cmd=json.loads((ROOT/'build/matrixgame/e2e524-text/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');j['strong_undefined']=[line for line in nm.splitlines() if ' U ' in line];assert not j['strong_undefined'];j['elf_sha256']=sha(elf)
   for symbol in ('native_surfaces::Session::Create(', 'native_surfaces::Lock(', 'DekoDevice::SurfaceSession()', 'DekoTextureStore::ReadImage(', 'DekoTextureStore::AllocateTarget(', 'CVectorObject::CalcShadowTextureWOMat(', 'CGame::Init('):assert symbol in nm,symbol
   j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  elif kind.startswith('solid-'):
   target=kind.removeprefix('solid-');prior=json.loads((ROOT/('build/matrixgame/e2e522/unions-'+target+'/result.json')).read_text());cmd=[s for s in prior['commands'][0]['command'] if not s.endswith('.cpp')]
   sources=['tests/test_matrixgame_original_solid_texture.cpp','port/matrixgame/native-core/texture_mips.cpp']+['port/matrixgame/native-engine/'+p for p in ('native_texture.cpp','platform_3g.cpp','legacy_texture_backend.cpp','unmanaged_texture_backend.cpp','native_surfaces.cpp','offscreen_backend.cpp')]+['build/matrixgame/e2e525-surfaces/sdk/source/MatrixLib/3G/'+p for p in ('OriginalManagedTexture.cpp','OriginalUnmanagedTexture.cpp')]
   first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/p) for p in sources];cmd[cmd.index('-o')+1]=linux(out/'solid');run(cmd,'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if target=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   j['stdout']=run(cmd+[linux(out/'solid')],'runtime')
  else:
   sources=['tests/test_matrixgame_native_surfaces.cpp','port/matrixgame/native-engine/native_surfaces.cpp','port/matrixgame/native-engine/offscreen_backend.cpp','port/matrixgame/native-engine/offscreen_scope.cpp','port/matrixgame/native-engine/platform_3g.cpp']
   cmd=json.loads((ROOT/('build/matrixgame/e2e522/queries-'+kind+'/result.json')).read_text())['commands'][0]['command'];cmd=[s for s in cmd if not s.endswith('.cpp')];first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/p) for p in sources];cmd[cmd.index('-o')+1]=linux(out/'surfaces');run(cmd,'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1'];
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   j['stdout']=run(cmd+[linux(out/'surfaces')],'runtime')
  paths=['port/matrixgame/native-engine/'+p for p in ('native_surfaces.hpp','native_surfaces.cpp','offscreen_backend.cpp','platform_switch.cpp','platform_switch.hpp','CMakeLists.txt','shadow_texture_backend.cpp')]+['port/matrixgame/deko/texture_store.cpp','port/matrixgame/deko/texture_store.hpp','tools/matrixgame_unmanaged_texture_overrides.py','tools/matrixgame_shadow_texture_overrides.py','port/matrixgame/native-engine/offscreen_scope.hpp','port/matrixgame/native-engine/offscreen_scope.cpp','tools/matrixgame_surface_checks.py']
  if kind!='sdk':paths+=sources
  j['source_sha256']={p:sha(ROOT/p) for p in paths};j['Switch_hardware']='HARDWARE_PENDING';j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()

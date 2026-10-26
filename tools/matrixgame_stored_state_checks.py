"""Original CDeviceState/native scalar tests and full SDK ELF, never an NRO."""
import argparse,hashlib,json,os,subprocess,time,sys
sys.stdout.reconfigure(encoding='utf-8')
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e529-stored-state'
SOURCE=ROOT/'build/matrixgame/e2e529-text-contract/sdk/source'
NATIVE=['native_device_state.cpp','native_stored_state.cpp','platform_3g.cpp']
COMMON=['port/matrixgame/native-engine/'+p for p in NATIVE]+['port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
TESTS=['tests/test_matrixgame_stored_state.cpp',str((SOURCE/'MatrixLib/3G/DeviceState.cpp').relative_to(ROOT))]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['host','arm64','windows','sdk']);kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'hardware':'HARDWARE_PENDING'}
 sources=COMMON+TESTS+([] if kind=='windows' else ['port/matrixgame/native-engine/map_device_backend.cpp'])
 tracked=sorted(set(sources+['tests/test_matrixgame_native_state.cpp','tests/probe_matrixgame_native_entry.cpp']+['port/matrixgame/native-engine/'+p for p in ('native_state_layout.hpp','native_device_state.hpp','native_stored_state.hpp','platform_switch.hpp','platform_switch.cpp','CMakeLists.txt')]+['port/matrixgame/deko/fixed_vertex.hpp','tools/matrixgame_stored_state_checks.py']))
 before={p:sha(ROOT/p) for p in tracked}
 includes=[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')]+[SOURCE/n for n in ('portable-core','MatrixLib/3G','MatrixLib/Base')]
 def run(cmd,label,env=None):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-2200:] if label!='symbols' else 'saved',flush=True);assert p.returncode==0;return p.stdout
 try:
  if kind in ('host','arm64'):
   flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffp-contract=off','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-pie','-no-pie','-pthread']
   if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
   target=out/'stored-state';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in sources],'-o',linux(target)],'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   j['stdout']=run(cmd+[linux(target)],'runtime');j['binary_sha256']=sha(target)
   # Existing COW/retention/OOM tests cover the expanded authoritative state too.
   target=out/'state-lease';lease_sources=[p for p in COMMON if not p.endswith(('native_stored_state.cpp','platform_3g.cpp'))]+['tests/test_matrixgame_native_state.cpp']
   run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in lease_sources],'-o',linux(target)],'lease-build')
   j['lease_stdout']=run(cmd+[linux(target)],'lease-runtime');j['lease_binary_sha256']=sha(target)
  elif kind=='windows':
   vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
   command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /MD /EHsc /std:c++20 /O2 /UNDEBUG '
   command+=' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(ROOT/p)+'"' for p in sources)
   command+=' /Fo"'+out.as_posix()+'/" /Fe"'+str(out/'stored-state.exe')+'"'
   script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');j['stdout']=run([out/'stored-state.exe'],'runtime');j['binary_sha256']=sha(out/'stored-state.exe')
  else:
   cmd=json.loads((ROOT/'build/matrixgame/e2e529-text-contract/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
   run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');j['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not j['strong_undefined']
   for symbol in ('CDeviceState::StoreState(', 'CDeviceState::RestoreState(', 'DekoDevice::StoredStates()', 'NativeStoredStateBackend::GetMaterial(', 'NativeRenderState::ValidateDraw(', 'CGame::Init('):assert symbol in nm,symbol
   j['elf_sha256']=sha(elf);j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  after={p:sha(ROOT/p) for p in tracked};assert before==after,'input changed during build/test'
  j['source_sha256']=after;j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

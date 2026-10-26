"""Addressed real original output paths/CFile and SDK native entry; no NRO."""
import argparse,hashlib,json,os,subprocess,time,uuid,sys,stat
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e527-output';SOURCE=BASE/'source'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['prepare','sdk','host','arm64','windows']);kind=ap.parse_args().kind;out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 def run(cmd,label,env=None,cwd=ROOT):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-3000:] if label!='symbols' else 'saved',flush=True);assert p.returncode==0;return p.stdout
 try:
  sources=['tests/test_matrixgame_output_paths.cpp','port/matrixgame/native-engine/output_paths_backend.cpp',str(SOURCE.relative_to(ROOT)/'MatrixGame/src/MatrixGamePathUtils.cpp')]
  if kind=='prepare':run([sys.executable,ROOT/'tools/matrixgame_engine_prepare.py','--output',SOURCE],'prepare')
  elif kind=='sdk':
   cmd=json.loads((ROOT/'build/matrixgame/e2e524-text/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');j['strong_undefined']=[line for line in nm.splitlines() if ' U ' in line];assert not j['strong_undefined'];j['elf_sha256']=sha(elf)
   for symbol in ('output_paths::NativeScope::NativeScope(', 'output_paths::FilesystemPath(', 'NativeInitRuntime::InitializeNativeGame(', 'CMinimap::RenderBackground(', 'PathToOutputFiles[abi:cxx11]('):assert symbol in nm,symbol
   j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  else:
   fixture=out/('run-'+uuid.uuid4().hex);j['filesystem_output_root']=str(fixture)
   if kind=='windows':
    prior=json.loads((ROOT/'build/matrixgame/e2e523-presentation/windows-frame/result.json').read_text());cmd=prior['commands'][0]['command'];prefix=cmd[:cmd.index(' /c ')];prefix=prefix.replace(str(ROOT/'build/matrixgame/e2e521/source'),str(SOURCE));command=prefix+' '+' '.join('"'+str(ROOT/p)+'"' for p in sources)+' /Fo"'+out.as_posix()+'/" /Fe"'+str(out/'output-paths.exe')+'" /link "'+str(ROOT/'build/matrixgame/e2e519/original-init-windows/core/Release/matrixgame-native-core.lib')+'" "'+str(ROOT/'build/matrixgame/e2e519/windows/zlib/Release/zs.lib')+'" shell32.lib ole32.lib user32.lib'
    script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');j['stdout']=run([out/'output-paths.exe',fixture],'runtime')
   else:
    prior=json.loads((ROOT/('build/matrixgame/e2e522/unions-'+kind+'/result.json')).read_text());cmd=[s for s in prior['commands'][0]['command'] if not s.endswith('.cpp')];cmd=[s.replace('/build/matrixgame/e2e521/source','/build/matrixgame/e2e527-output/source') for s in cmd];first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/p) for p in sources];cmd[cmd.index('-o')+1]=linux(out/'output-paths');run(cmd,'build')
    cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
    if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
    j['stdout']=run(cmd+[linux(out/'output-paths'),linux(fixture)],'runtime')
   j['output_sha256']={}
   for p in fixture.rglob('*'):
    st=p.lstat()
    if stat.S_ISREG(st.st_mode) and not (getattr(st,'st_file_attributes',0)&0x400):j['output_sha256'][str(p.relative_to(ROOT))]=sha(p)
   assert len(j['output_sha256'])==1
  paths=['port/matrixgame/native-engine/'+p for p in ('output_paths_backend.hpp','output_paths_backend.cpp','original_game.hpp','original_game.cpp','init_runtime.hpp','init_runtime.cpp')]+['tools/matrixgame_output_paths_overrides.py','tools/matrixgame_minimap_overrides.py','tools/matrixgame_output_checks.py','tests/test_matrixgame_output_paths.cpp','tests/probe_matrixgame_native_entry.cpp']
  if kind not in ('sdk','prepare'):paths+=sources
  j['source_sha256']={p:sha(ROOT/p) for p in paths};j['Switch_hardware']='HARDWARE_PENDING';j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()

"""Original UI font oracle/native Text::Render and SDK ELF. Never produces an NRO."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e524-text'
SOURCES=['tests/test_matrixgame_native_text.cpp']+['port/matrixgame/native-engine/'+n for n in ('platform_3g.cpp','text_backend.cpp','native_text.cpp','native_text_backend.cpp','native_texture.cpp','legacy_texture_backend.cpp')]+['build/matrixgame/e2e523-presentation/sdk/source/MatrixGame/src/Text/'+n for n in ('Render.cpp','Font.cpp','Parser.cpp')]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['windows','host','arm64','sdk']);ap.add_argument('--output',type=Path,default=BASE);args=ap.parse_args();kind=args.kind;out=args.output.resolve()/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'hardware':'HARDWARE_PENDING'}
 def run(cmd,label,env=None):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');report['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-started});print(label,p.returncode,(p.stdout+p.stderr)[-3000:] if label!='symbols' else 'saved',flush=True);assert p.returncode==0;return p.stdout
 try:
  if kind=='windows':
   dx=ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8/build/native';vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
   command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /utf-8 /EHsc /std:c++20 /MD /O2 /UNDEBUG /I"{dx/"include"}" /I"{ROOT/"port/matrixgame/native-engine"}" '
   command+=' '.join('"'+str(ROOT/p)+'"' for p in ['tests/matrixgame_text_font_export.cpp','port/matrixgame/native-engine/native_text.cpp'])+f' /Fo"{out.as_posix()}/" /Fe"{out/"font-oracle.exe"}" /link d3d9.lib user32.lib gdi32.lib "{dx/"release/lib/x86/d3dx9.lib"}"'
   script=out/'export.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');report['stdout']=run([out/'font-oracle.exe',ROOT/'upstream/matrixgame/MatrixGame/Rangers.ttf',out],'runtime')
   previous=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text())['commands'][0]['command'];previous=previous.replace(str(ROOT/'build/matrixgame/e2e522/clean-sdk-release/source/MatrixGame/src/OriginalMapPrepare.cpp'),str(ROOT/'port/matrixgame/native-engine/text_backend.cpp')).replace(str(ROOT/'build/matrixgame/e2e522/windows-frame/OriginalMapPrepare.obj'),str(out/'text_backend.obj'));bridge=out/'bridge.cmd';bridge.write_text(previous+'\n');run(['cmd.exe','/d','/c',bridge],'bridge-compile')
  elif kind in ('host','arm64'):
   prior=ROOT/('build/matrixgame/e2e523-presentation/text-boundary/result.json' if kind=='host' else 'build/matrixgame/e2e522/queries-arm64/result.json');cmd=json.loads(prior.read_text())['commands'][0]['command'];cmd=[s for s in cmd if not s.endswith('.cpp')];first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/p) for p in SOURCES];cmd[cmd.index('-o')+1]=linux(out/'native-text');run(cmd,'build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1'];
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   report['stdout']=run(cmd+[linux(out/'native-text'),linux(BASE/'windows')],'runtime');report['binary_sha256']=sha(out/'native-text')
  else:
   cmd=json.loads((ROOT/'build/matrixgame/e2e521/font/cmake-configure.json').read_text());cmd[cmd.index('-B')+1]=str(out);cmd+=['-DMATRIXGAME_TEXT_FONT_ASSETS='+str(BASE/'windows')];env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
   elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');report['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not report['strong_undefined'];report['required_symbols']={}
   for symbol in ('CGame::Init(', 'CMatrixMap::PrepareMap(', 'text_backend::DeviceIdentity()', 'text_backend::EndScene(', 'text_backend::native::Draw(', 'text_backend::native::OriginalAsset(', 'Text::Render('):
    found=[s for s in nm.splitlines() if symbol in s];assert found,symbol;report['required_symbols'][symbol]=found[:2]
   report['elf_sha256']=sha(elf);report['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  report['assets_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (BASE/'windows').glob('font*.*') if p.suffix in ('.bin','.golden','.sfnt')};report['rangers_ttf_sha256']=sha(ROOT/'upstream/matrixgame/MatrixGame/Rangers.ttf');report['source_sha256']={p:sha(ROOT/p) for p in SOURCES+['port/matrixgame/native-engine/native_text.hpp','port/matrixgame/native-engine/native_text_asset.cpp','port/matrixgame/native-engine/CMakeLists.txt','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/platform_switch.hpp','tests/test_matrixgame_native_text_boundary.cpp','tests/matrixgame_text_font_export.cpp','tools/matrixgame_text_checks.py']};report['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()

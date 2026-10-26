"""Addressed production presentation transaction tests and complete SDK ELF. No NRO."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e523-presentation'
SOURCES=['tests/test_matrixgame_native_presentation.cpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
HEADERS=['port/matrixgame/native-engine/native_presentation.hpp','port/matrixgame/native-engine/native_texture.hpp','port/matrixgame/native-engine/native_device_state.hpp','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/platform_switch.hpp','port/matrixgame/native-engine/CMakeLists.txt','port/matrixgame/native-engine/native_frame_scope.hpp','port/matrixgame/native-engine/init_runtime.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows','sdk'));kind=ap.parse_args().kind;out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[]}
    def run(cmd,label,env=None):
        started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-started});print(label,p.returncode,'symbol table saved' if label=='symbols' else (p.stdout+p.stderr)[-3000:],flush=True);assert p.returncode==0;return p.stdout
    try:
        if kind in ('host','arm64'):
            prior=json.loads((ROOT/('build/matrixgame/e2e522/queries-'+kind+'/result.json')).read_text());cmd=prior['commands'][0]['command'].copy();cmd=[s for s in cmd if not s.endswith('.cpp')];first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/n) for n in SOURCES];cmd[cmd.index('-o')+1]=linux(out/'presentation');run(cmd,'build')
            cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            result['stdout']=run(cmd+[linux(out/'presentation')],'runtime')
        elif kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            includes=[ROOT/'port/matrixgame/native-engine',ROOT/'port/matrixgame/native-core']
            command='call "'+vs+'" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /MD /O2 /UNDEBUG '+ ' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(ROOT/n)+'"' for n in SOURCES)+' /Fo"'+out.as_posix()+'/" /Fe"'+str(out/'presentation.exe')+'"'
            script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');result['stdout']=run([out/'presentation.exe'],'runtime')
        else:
            cmd=json.loads((ROOT/'build/matrixgame/e2e521/font/cmake-configure.json').read_text());cmd[cmd.index('-B')+1]=str(out);env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');run(cmd,'configure',env);run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build',env)
            elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');undefined=[s for s in nm.splitlines() if ' U ' in s];assert not undefined;result['strong_undefined']=undefined
            for symbol in ('CGame::Init(', 'CGame::ApplyVideoParams(', 'DekoDevice::ReadCreationParameters()', 'DekoDevice::ReadAdapterDisplayMode(', 'DekoDevice::ResetPresentation(', 'SwitchWindow::ResizeOuter(', 'SwitchWindow::MoveFullscreen(', 'presentation::RestoreLease::Restore()'):
                assert symbol in nm,symbol
            result['elf_sha256']=sha(elf);result['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
        result['source_sha256']={n:sha(ROOT/n) for n in SOURCES+HEADERS+[str(Path(__file__).resolve().relative_to(ROOT))]};result['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()

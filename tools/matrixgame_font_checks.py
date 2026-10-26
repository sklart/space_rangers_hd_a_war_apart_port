"""Check exact original native debug font, HAL goldens and original SDK link. Never builds NRO."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/font'
SOURCES=['port/matrixgame/native-engine/native_debug_font.cpp','port/matrixgame/native-engine/debug_font_backend.cpp',
    'port/matrixgame/native-engine/platform_3g.cpp','build/matrixgame/e2e521/source/MatrixGame/src/MatrixDebugInfo.cpp','tests/test_matrixgame_native_font.cpp']

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows','sdk'));kind=ap.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'Switch_runtime':False}
    def run(cmd,label,cwd=ROOT):
        proc=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',
            env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'))
        report['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':proc.returncode});(out/(label+'.log')).write_text(proc.stdout+proc.stderr,encoding='utf-8')
        print(kind,label,proc.returncode,(proc.stdout+proc.stderr)[-1400:] if proc.returncode or label=='runtime' else '',flush=True)
        if proc.returncode:raise RuntimeError(label)
        return proc.stdout
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    sources=[ROOT/n for n in SOURCES]
    try:
        if kind in ('host','arm64'):
            cxx='g++' if kind=='host' else 'aarch64-linux-gnu-g++-15';prefix=['wsl','--exec']
            includes=[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko','third_party/directxmath/Inc','third_party/sal')]+[ROOT/'build/matrixgame/e2e521/source/MatrixLib'/n for n in ('3G','Base','Bitmap')]+[ROOT/'build/matrixgame/e2e521/source/MatrixGame/src'];target=out/'native-font'
            flags=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-std=c++20','-O1','-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie']
            if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            run([*prefix,cxx,*flags,*['-I'+linux(p) for p in includes],*map(linux,sources),'-o',linux(target)],'build')
            cmd=[*prefix,'env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            report['stdout']=run([*cmd,linux(target),linux(BASE/'windows/original-font.bin'),linux(BASE/'windows/original-font.bin.goldens')],'runtime')
        elif kind=='windows':
            d=ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8/build/native'
            sources=[ROOT/'tests/matrixgame_native_font_windows.cpp',ROOT/'port/matrixgame/native-engine/native_debug_font.cpp']
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /MD /O2 /UNDEBUG /I"{d/"include"}" /I"{ROOT/"port/matrixgame/native-engine"}" '
            command+=' '.join('"'+str(p)+'"' for p in sources)+f' /Fo"{out.as_posix()}/" /Fe"{out/"font-oracle.exe"}" /link d3d9.lib user32.lib gdi32.lib "{d/"release/lib/x86/d3dx9.lib"}"'
            script=out/'export.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build')
            report['stdout']=run([out/'font-oracle.exe',out/'original-font.bin'],'runtime')
        else:
            prior=json.loads((ROOT/'build/matrixgame/e2e521/cursor/sdk/result.json').read_text())
            template=next(c['command'].copy() for c in prior['commands'] if c['label']=='platform_switch')
            sources=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_debug_font.cpp','native_debug_font_asset.cpp','debug_font_backend.cpp','platform_switch.cpp')]+[ROOT/'tests/probe_matrixgame_native_entry.cpp',ROOT/'tests/matrixgame_native_font_probe.cpp'];objects=[]
            import shutil
            shutil.copyfile(BASE/'windows/original-font.bin',out/'original-font.bin')
            run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-objcopy.exe','-I','binary','-O','elf64-littleaarch64','-B','aarch64','--rename-section','.data=.rodata,alloc,load,readonly,data,contents','original-font.bin','original-font.o'],'font-binary',out)
            objects.append(out/'original-font.o')
            for source in sources:
                cmd=template.copy();obj=out/(source.stem+'.o');cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj);run(cmd,source.stem);objects.append(obj)
            link=next(c['command'].copy() for c in prior['commands'] if c['label']=='link')
            replaced={p.stem for p in sources};link=[s for s in link if Path(s).suffix!='.o' or Path(s).stem not in replaced]
            first=next(i for i,s in enumerate(link) if s.endswith('.a'));link[first:first]=list(map(str,objects));link[link.index('-o')+1]=str(out/'native-entry.elf')
            run(link,'link');report['link_inputs_sha256']={str(Path(s)):sha(Path(s)) for s in link if Path(s).suffix in ('.a','.o') and Path(s).is_file()}
            nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',out/'native-entry.elf'],'symbols')
            report['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not report['strong_undefined']
            report['required_symbols']={}
            for symbol in ('CGame::Init(','Data::Load(','Font::Create(','Font::Draw(','debug_font::Lost(','debug_font::Reset(',
                'Font::Rebind(','Session::Close()','DekoDevice::DebugFontSession()','debug_font::Create(',
                'native::OriginalAsset()', 'CMatrixDebugInfo::OnLostDevice()','CMatrixDebugInfo::OnResetDevice()', 'CMatrixDebugInfo::CMatrixDebugInfo()','CMatrixMap::PrepareMap('):
                found=[s for s in nm.splitlines() if symbol in s];assert found,symbol;report['required_symbols'][symbol]=found[:3]
            report['elf_sha256']=sha(out/'native-entry.elf')
        headers=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_debug_font.hpp','debug_font_backend.hpp','platform_switch.hpp')]
        report['input_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (BASE/'windows/original-font.bin',BASE/'windows/original-font.bin.goldens')}
        report['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+headers+[Path(__file__).resolve()]};report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

"""Addressed BC1 software checks, separate from Switch execution."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e522'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','perf','arm64','windows','sdk'));kind=ap.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_runtime':False};sources=[]
    def run(cmd,label,cwd=ROOT,env=None):
        start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',env=env or dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'))
        result['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');print(kind,label,p.returncode,(p.stdout+p.stderr)[-3500:] if label=='runtime' or p.returncode else '',flush=True)
        assert p.returncode==0,label
        return p.stdout
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    try:
        if kind in ('host','perf','arm64'):
            prior=json.loads((ROOT/('build/matrixgame/e2e521/residency/'+('arm64' if kind=='arm64' else 'host')+'/result.json')).read_text())
            cmd=next(c['command'].copy() for c in prior['commands'] if c['label']=='build-link');cmd=[s for s in cmd if not s.endswith('.cpp')]
            sources=[ROOT/n for n in ('port/matrixgame/native-core/texture_mips.cpp','port/matrixgame/native-engine/native_texture.cpp','port/matrixgame/native-engine/platform_3g.cpp','tests/test_matrixgame_bc1.cpp','tests/matrixgame_allocation_fault.cpp')]
            first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=list(map(linux,sources));cmd[cmd.index('-o')+1]=linux(out/'bc1')
            if kind=='perf':cmd=[s for s in cmd if not s.startswith('-fsanitize=') and s!='-O1'];cmd=[linux(BASE/'clean-host-release/libmatrixgame-native-core.a') if s.endswith('libmatrixgame-native-core.a') else s for s in cmd];cmd+=['-O3']
            run(cmd,'build')
            command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind in ('perf','arm64'):command+=['/usr/bin/time','-v']
            if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            command+=[linux(out/'bc1'),linux(BASE/'windows-frame'),linux(out)]
            result['stdout']=run(command,'runtime')
        elif kind=='windows':
            d=ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8/build/native';source=ROOT/'tests/matrixgame_bc1_windows.cpp';sources=[source]
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /MD /O2 /UNDEBUG /I"{d/"include"}" /I"{ROOT/"port/matrixgame/native-engine"}" "{source}" /Fo"{out.as_posix()}/" /Fe"{out/"bc1-oracle.exe"}" /link d3d9.lib user32.lib gdi32.lib "{d/"release/lib/x86/d3dx9.lib"}"'
            script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');result['stdout']=run([out/'bc1-oracle.exe',BASE/'windows-frame',BASE/'host'],'runtime')
        else:
            run([__import__('sys').executable,ROOT/'tools/matrixgame_e2e522_build.py','sdk'],'cmake-build')
            build=BASE/'clean-sdk-release';elf=build/'matrixgame-native-entry.elf'
            nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols');result['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not result['strong_undefined']
            for symbol in ('CGame::Init(', 'CMatrixMap::PrepareMap(', 'TextureAsset::CompressBC1(', 'DekoDevice::NativeReadViewport()', 'DekoDevice::NativeCooperativeLevel()', 'DekoDevice::NativeClear(', 'DekoDevice::NativePresent()', 'BuildOriginalTextureUnions('):
                assert symbol in nm,symbol
            result['elf_path']=str(elf.relative_to(ROOT));result['elf_sha256']=sha(elf)
            result['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in build.rglob('*') if p.is_file() and p.suffix in ('.a','.obj','.o')}
            sources=[ROOT/n for n in ('port/matrixgame/native-core/texture_mips.cpp','port/matrixgame/native-engine/native_texture.cpp','port/matrixgame/deko/texture_store.cpp','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/platform_switch.hpp','port/matrixgame/native-engine/native_map_queries.hpp','port/matrixgame/native-engine/bitmap_array_owner.hpp','tools/matrixgame_map_prepare_overrides.py','port/matrixgame/native-engine/native_frame_scope.hpp','port/matrixgame/native-engine/offscreen_backend.cpp','port/matrixgame/native-engine/transition_backend.cpp','port/matrixgame/native-engine/map_device_backend.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/fixed_vertex.hpp','port/matrixgame/native-engine/CMakeLists.txt','tests/probe_matrixgame_native_entry.cpp','port/matrixgame/native-engine/original_texture_unions.hpp','tools/matrixgame_engine_prepare.py','build/matrixgame/e2e522/clean-sdk-release/source/MatrixGame/src/OriginalTextureUnions.cpp')]
        headers=[ROOT/'port/matrixgame/native-core/texture_asset.hpp',ROOT/'port/matrixgame/native-engine/native_texture.hpp']
        result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+headers+[Path(__file__).resolve()]};result['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

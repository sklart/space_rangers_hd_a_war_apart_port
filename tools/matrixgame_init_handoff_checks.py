"""Short CPU integration of real state/frame owners and extracted Init handoff."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['tests/test_matrixgame_init_handoff.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-init-r7-checks'/kind;parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();report={'status':'INCOMPLETE','commands':[],'scope':'CPU endpoint model, real StateStore/FrameScope; no GPU or original Init execution'}
    source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text(encoding='utf-8')
    def body(start):
        a=source.index(start);a=source.index('{',a);depth=1;b=a+1
        while depth:
            if source[b]=='{':depth+=1
            elif source[b]=='}':depth-=1
            b+=1
        return source[a:b]
    (out/'flush-recording.inc').write_text('void FlushRecording()'+body('    void FlushRecording()')+'\n',encoding='utf-8')
    (out/'complete-init.inc').write_text('void CompleteNativeInitializationWork()'+body('void DekoDevice::CompleteNativeInitializationWork()')+'\n',encoding='utf-8')
    # Explicit app boundary: no status snapshot or loop can precede completion.
    app=(ROOT/'tests/matrixgame_native_diagnostic_main.cpp').read_text(encoding='utf-8');a=app.index('runtime.InitializeNativeGame(options);');b=app.index('RunOriginalNativeLoop();',a);handoff=app[a:b]
    assert handoff.index('CompleteNativeInitializationWork();')<handoff.index('dg::RenderStatus(')
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-3000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label;return log
    try:
        includes=[out,*[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')]];binary=out/'init-handoff'
        if kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            command=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG '+' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(ROOT/p)+'"' for p in SOURCES)+f' /Fo"{out.as_posix()}/" /Fe"{binary}.exe"'
            script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');runtime=[str(binary)+'.exe']
        else:
            flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie','-fsanitize=address,undefined' if kind=='host' else '-fsanitize=undefined','-fno-omit-frame-pointer']
            # Avoid instrumented std::type_info::operator== interposing inside
            # libubsan on this g++15/QEMU runtime. Keep UBSan/vptr enabled.
            if kind=='arm64':
                version=out/'local-typeinfo.map';version.write_text('{ local: _ZNKSt9type_infoeqERKS_; };\n');flags+=['-Wl,--version-script='+linux(version)]
            run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in SOURCES],'-o',linux(binary)],'build')
            runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            runtime+=[linux(binary)]
        report['stdout']=run(runtime,'runtime');assert 'INIT_HANDOFF PASS' in report['stdout']
        report['status']='PASS INIT HANDOFF; CPU MODEL ONLY; HARDWARE_PENDING'
    finally:
        paths=SOURCES+['port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/platform_switch.hpp','port/matrixgame/native-engine/native_frame_scope.hpp','tests/matrixgame_native_diagnostic_main.cpp','tools/matrixgame_init_handoff_checks.py']
        report['source_sha256']={p:sha(ROOT/p) for p in paths};report['extracted_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.glob('*.inc')};(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

"""Single-sample D3D9 state semantics; focused current production builds."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from matrixgame_stored_state_checks import ROOT, COMMON, sha, linux

BASE=ROOT/'build/matrixgame/e2e562-single-sample'
SOURCE=ROOT/'build/matrixgame/e2e561-unmanaged-bitmap/sdk/source'


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('kind',choices=('baseline','host','arm64','windows'))
    parser.add_argument('--suite',choices=('single','stored','replay'),default='single')
    args=parser.parse_args();kind=args.kind
    assert kind!='baseline' or args.suite=='single'
    out=BASE/kind
    if args.suite!='single':out=out/args.suite
    out.mkdir(parents=True,exist_ok=True)
    report={'status':'INCOMPLETE','commands':[]}
    def run(cmd,label):
        cmd=list(map(str,cmd));start=time.monotonic()
        p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
        log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8')
        report['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-start})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or 'runtime' in label else '',flush=True)
        assert p.returncode==0,label
        return log
    try:
        sources=[s for s in COMMON if not s.endswith(('native_stored_state.cpp','platform_3g.cpp'))]+['tests/test_matrixgame_single_sample.cpp']
        captures=[]
        if args.suite=='stored':
            sources=COMMON+['tests/test_matrixgame_stored_state.cpp',str((SOURCE/'MatrixLib/3G/DeviceState.cpp').relative_to(ROOT))]
            if kind!='windows':sources+=['port/matrixgame/native-engine/map_device_backend.cpp']
        elif args.suite=='replay':
            sources=[s for s in COMMON if not s.endswith(('native_stored_state.cpp','platform_3g.cpp'))]+['tests/test_matrixgame_actual_draw_replay.cpp']
            captures=sorted((ROOT/'build/matrixgame/e2e543-journal/map-state/windows').glob('draw-states-*.bin'));assert len(captures)==2
            report['captures_sha256']={str(p.relative_to(ROOT)):sha(p) for p in captures}
        includes=[ROOT/'port/matrixgame'/s for s in ('native-engine','native-core','deko')]+[SOURCE/s for s in ('portable-core','MatrixLib/3G','MatrixLib/Base')]
        report['source_sha256']={s:sha(ROOT/s) for s in sources}
        report['source_sha256']['port/matrixgame/native-engine/native_device_state.hpp']=sha(ROOT/'port/matrixgame/native-engine/native_device_state.hpp')
        if args.suite=='replay':report['source_sha256']['tests/matrixgame_actual_draw_state.hpp']=sha(ROOT/'tests/matrixgame_actual_draw_state.hpp')
        if kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            exe=out/'single-sample.exe'
            command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /MD /EHsc /std:c++20 /O2 /UNDEBUG /DMATRIXGAME_SINGLE_SAMPLE_CANDIDATE '
            command+=' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(ROOT/s)+'"' for s in sources)
            command+=f' /Fo"{out.as_posix()}/" /Fe"{exe}"'
            script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build')
            runtime=[exe,*captures]
        else:
            flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffp-contract=off','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-pie','-no-pie','-pthread']
            if kind!='arm64':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            if kind!='baseline':flags+=['-DMATRIXGAME_SINGLE_SAMPLE_CANDIDATE']
            exe=out/'single-sample'
            run(['wsl','--exec','aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/s) for s in sources],'-o',linux(exe)],'build')
            runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            runtime+=[linux(exe),*map(linux,captures)]+(['--baseline'] if kind=='baseline' else [])
        report['stdout']=run(runtime,'runtime');report['binary_sha256']=sha(exe)
        report['status']='PASS; BASELINE REPRODUCED' if kind=='baseline' else 'PASS; CPU STATE ONLY'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()

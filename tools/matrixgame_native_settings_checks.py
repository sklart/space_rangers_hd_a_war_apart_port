"""Native entry settings: exact scalar preservation and input rejection."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_single_sample_checks import linux

BASE=ROOT/'build/matrixgame/e2e565-native-settings'
ENGINE=ROOT/'build/matrixgame/e2e564-pad-repeat/sdk/source'


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64','windows'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[]}
    includes=[ROOT/'port/matrixgame/native-engine',ENGINE/'MatrixGame/src',*[ENGINE/'MatrixLib'/name for name in ('3G','Base','Bitmap')],ENGINE/'portable-core',ROOT/'port/matrixgame/native-core',ROOT/'port/matrixgame/deko',ROOT/'port/matrixgame/third_party/directxmath/Inc',ROOT/'port/matrixgame/third_party/sal']
    sources=['tests/test_matrixgame_native_settings.cpp','port/matrixgame/native-engine/native_game_settings.hpp','port/matrixgame/native-engine/original_game.hpp']
    before={name:sha(ROOT/name) for name in sources};result['source_sha256']=before
    def run(command,label):
        command=list(map(str,command));start=time.monotonic();p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'seconds':time.monotonic()-start})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        if kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat';exe=out/'settings.exe'
            command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /O2 /UNDEBUG '
            command+=' '.join('/I"'+str(p)+'"' for p in includes)+f' "{ROOT/sources[0]}" /Fo"{out.as_posix()}/" /Fe"{exe}"'
            script=out/'build.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'build');runtime=[exe,out/'settings.bin']
        else:
            flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie','-ffp-contract=off']
            if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            exe=out/'settings';run(['wsl','--exec','aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++',*flags,*['-I'+linux(p) for p in includes],linux(ROOT/sources[0]),'-o',linux(exe)],'build')
            runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            runtime+=[linux(exe),linux(out/'settings.bin')]
        result['stdout']=run(runtime,'runtime');result['binary_sha256']=sha(exe);result['observations_sha256']=sha(out/'settings.bin')
        assert before=={name:sha(ROOT/name) for name in sources}
        result['status']='PASS; SETTINGS PREPARATION CPU ONLY'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()

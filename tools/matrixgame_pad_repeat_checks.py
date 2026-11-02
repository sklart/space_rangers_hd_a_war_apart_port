"""Controller edge dispatch and physical keyboard typematic contracts."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_single_sample_checks import linux

BASE=ROOT/'build/matrixgame/e2e564-pad-repeat'


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('baseline','host','arm64'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[]}
    names=['port/matrixgame/native-engine/native_input.cpp','port/matrixgame/native-engine/native_input.hpp','tests/test_matrixgame_pad_repeat.cpp']
    before={name:sha(ROOT/name) for name in names};result['source_sha256']=before
    if kind=='baseline':
        snapshot=out/'source';snapshot.mkdir(exist_ok=True)
        for name in names:
            target=snapshot/Path(name).name
            if not target.exists():target.write_bytes((ROOT/name).read_bytes())
            assert sha(target)==before[name]
    def run(command,label):
        command=list(map(str,command));start=time.monotonic();p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'seconds':time.monotonic()-start})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
        if kind!='arm64':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        exe=out/'repeat';run(['wsl','--exec','aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++',*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),linux(ROOT/names[0]),linux(ROOT/names[2]),'-o',linux(exe)],'build')
        command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
        if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
        result['stdout']=run(command+[linux(exe)]+(['--baseline'] if kind=='baseline' else []),'runtime')
        assert before=={name:sha(ROOT/name) for name in names};result['binary_sha256']=sha(exe)
        result['status']='PASS; BASELINE REPRODUCED' if kind=='baseline' else 'PASS; INPUT DISPATCH ONLY'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()

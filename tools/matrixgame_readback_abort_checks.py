"""Actual AbortNativeFrame prefix ownership regression, baseline and candidate."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha,read
BASE=ROOT/'build/matrixgame/e2e566-readback-abort'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    source=ROOT/'port/matrixgame/native-engine/platform_switch.cpp';baseline=BASE/'platform_switch-baseline.cpp'
    metadata=read(ROOT/'build/matrixgame/e2e565-native-settings/sdk/native-diagnostic-build.json')
    assert sha(baseline)==metadata['own_source_sha256']['port/matrixgame/native-engine/platform_switch.cpp']
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(label,p.returncode,log[-3000:] if p.returncode or label.endswith('runtime') else '',flush=True);assert p.returncode==0
        return log
    try:
        for mode,path in (('baseline',baseline),('candidate',source)):
            text=path.read_text();start=text.index('\n',text.index('DeviceResult DekoDevice::AbortNativeFrame()noexcept{'))+1;end=text.index('    try{',start)
            prefix=text[start:end];folder=out/mode;folder.mkdir(exist_ok=True);(folder/'abort-readback-prefix.inc').write_text(prefix)
            if mode=='candidate':assert prefix.index('v.primary_readback={};')<prefix.index('if(!v.queue')
            exe=folder/'readback-abort';flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
            if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(folder),linux(ROOT/'tests/test_matrixgame_readback_abort.cpp'),'-o',linux(exe)],mode+'-build')
            command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            report[mode+'_stdout']=run(command+[linux(exe)]+(['baseline'] if mode=='baseline' else []),mode+'-runtime')
            report[mode+'_binary_sha256']=sha(exe)
        report['status']='PASS; BASELINE CALLBACK RETENTION REPRODUCED; CANDIDATE CANCELS BOTH'
    finally:
        report['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (source,baseline,ROOT/'tests/test_matrixgame_readback_abort.cpp',Path(__file__).resolve())}
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

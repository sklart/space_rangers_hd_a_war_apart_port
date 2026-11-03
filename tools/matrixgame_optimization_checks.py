"""Compare actual CPU decoder/validator at O0/O2; retain assertions and errors."""
import argparse,hashlib,json,re,subprocess,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FILES=['tests/benchmark_matrixgame_native_optimization.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-opt-r9-checks'/kind;parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();result={'status':'INCOMPLETE','commands':[],'scope':'real production CPU state/decoder code; synthetic valid workload; not GPU/Switch FPS'}
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-3000:] if p.returncode or label.startswith('runtime') else '',flush=True);assert p.returncode==0,label;return log
    try:
        includes=[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')];measurements=[]
        for opt in (('O0','O2') if kind=='host' else ('O2',)):
            exe=out/opt
            if kind=='windows':
                vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
                cmd=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG /fp:strict '+' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(ROOT/p)+'"' for p in FILES)+f' /Fo"{out.as_posix()}/" /Fe"{exe}.exe"'
                script=out/'build.cmd';script.write_text(cmd+'\n');run(['cmd.exe','/d','/c',script],'build-'+opt);runtime=[str(exe)+'.exe']
            else:
                flags=['-std=c++20','-'+opt,'-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie']
                if kind=='arm64':
                    version=out/'local-typeinfo.map';version.write_text('{ local: _ZNKSt9type_infoeqERKS_; };\n');flags+=['-fsanitize=undefined','-fno-sanitize-recover=undefined','-Wl,--version-script='+linux(version)]
                run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in FILES],'-o',linux(exe)],'build-'+opt)
                runtime=['wsl','--exec','env','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
                if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
                runtime+=[linux(exe)]
            for sample in range(3 if kind=='host' else 1):
                log=run(runtime,'runtime-'+opt+'-'+str(sample));fields={k:int(v) for k,v in re.findall(r'(validate_calls|decode_vertices|validate_us|decode_us|checksum|guards)=(\d+)',log)}
                assert fields['validate_calls']==20000 and fields['decode_vertices']==200000 and fields['guards']==4 and 'ASSERTIONS_ON' in log
                measurements.append({'optimization':opt,**fields})
        assert len({r['checksum'] for r in measurements})==1
        result['measurements']=measurements;result['median_us']={opt:{name:statistics.median(m[name] for m in measurements if m['optimization']==opt) for name in ('validate_us','decode_us')} for opt in {m['optimization'] for m in measurements}}
        result['status']='PASS NATIVE CPU O2 PROFILE; EXACT CHECKSUM AND FOUR REJECTION GUARDS; NOT SWITCH FPS'
    finally:
        result['source_sha256']={p:sha(ROOT/p) for p in FILES+['port/matrixgame/native-engine/native_device_state.hpp','port/matrixgame/deko/fixed_vertex.hpp','tools/matrixgame_optimization_checks.py']};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

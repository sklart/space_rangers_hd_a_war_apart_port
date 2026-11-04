"""Compare frozen r28 and current state CPP at O2: semantics and CPU timings."""
import argparse,hashlib,json,re,subprocess,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GIT='C:/Program Files/Git/cmd/git.exe';FROZEN='7ca1c4db4d896bfb6bcc2ba348ff34fe54708a26'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows','benchmark'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-vertex-r29-checks'/kind;parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();j={'status':'INCOMPLETE','commands':[],'scope':'real frozen/candidate CPU validator; no Switch/GPU/FPS'}
    frozen=out/'frozen_native_device_state.cpp';frozen.write_bytes(subprocess.check_output([GIT,'show',FROZEN+':port/matrixgame/native-engine/native_device_state.cpp']))
    for name in ('native_device_state.hpp','native_state_layout.hpp'):
        assert (ROOT/'port/matrixgame/native-engine'/name).read_bytes().replace(b'\r\n',b'\n')==subprocess.check_output([GIT,'show',FROZEN+':port/matrixgame/native-engine/'+name]).replace(b'\r\n',b'\n')
    test='tests/benchmark_matrixgame_scalar_rules.cpp' if kind=='benchmark' else 'tests/test_matrixgame_sampler_lookup.cpp'
    common=['port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp'];current='port/matrixgame/native-engine/native_device_state.cpp'
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-2500:] if p.returncode or label.startswith('runtime') else '',flush=True);assert p.returncode==0,label;return log
    try:
        programs={};measurements=[];includes=[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')]
        for variant in ('frozen','candidate'):
            files=[ROOT/test,frozen if variant=='frozen' else ROOT/current,*[ROOT/p for p in common]];exe=out/variant
            if kind=='windows':
                obj=out/(variant+'-objects');obj.mkdir();vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
                cmd=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG /fp:strict '+' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(p)+'"' for p in files)+f' /Fo"{obj.as_posix()}/" /Fe"{exe}.exe"'
                script=out/(variant+'.cmd');script.write_text(cmd+'\n');run(['cmd.exe','/d','/c',script],'build-'+variant);programs[variant]=[str(exe)+'.exe']
            else:
                flags=['-std=c++20','-O2','-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie','-fno-omit-frame-pointer']
                if kind=='host':flags+=['-fsanitize=address,undefined']
                if kind=='arm64':
                    version=out/'local-typeinfo.map';version.write_text('{ local: _ZNKSt9type_infoeqERKS_; };\n');flags+=['-fsanitize=undefined','-Wl,--version-script='+linux(version)]
                run(['wsl','--exec','aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++',*flags,*['-I'+linux(p) for p in includes],*map(linux,files),'-o',linux(exe)],'build-'+variant)
                runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
                if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
                programs[variant]=runtime+[linux(exe)]
        if kind=='benchmark':
            for i,variant in enumerate(('frozen','candidate','candidate','frozen','frozen','candidate')):
                log=run(programs[variant],'runtime-'+variant+'-'+str(i));m={k:int(v) for k,v in re.findall(r'(set_us|prepare_us|checksum)=(\d+)',log)};measurements.append({'variant':variant,**m})
            assert len({m['checksum'] for m in measurements})==1
            j['measurements']=measurements;j['median_us']={v:{k:statistics.median(m[k] for m in measurements if m['variant']==v) for k in ('set_us','prepare_us')} for v in programs}
        else:
            outputs={v:run(cmd,'runtime-'+v) for v,cmd in programs.items()};assert outputs['candidate']==outputs['frozen'],'frozen behavior differs'
            assert 'SAMPLER_LOOKUP' in outputs['candidate'];j['semantic_stdout']=outputs['candidate']
        j['status']='PASS FROZEN R28/CANDIDATE CPU O2 COMPARISON; GUARDS RETAINED; HARDWARE_PENDING'
    finally:
        paths=[test,current,*common,'port/matrixgame/native-engine/native_device_state.hpp','port/matrixgame/native-engine/native_state_layout.hpp','tools/matrixgame_vertex_dispatch_checks.py'];j['source_sha256']={p:sha(ROOT/p) for p in paths};j['frozen_commit']=FROZEN;j['frozen_cpp_sha256']=sha(frozen);(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

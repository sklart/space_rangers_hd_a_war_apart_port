"""Compare frozen r32 and current state CPP at O2: semantics and CPU timings."""
import io,zipfile,argparse,hashlib,json,re,subprocess,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GIT='C:/Program Files/Git/cmd/git.exe';FROZEN='3d4c8c8a2dbffb60cd0023df9f988d1b206d3324'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows','benchmark'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-ctor-r33-checks'/kind;parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();j={'status':'INCOMPLETE','commands':[],'scope':'real frozen/candidate CPU validator; no Switch/GPU/FPS'}
    tree=out/'frozen-tree';tree.mkdir()
    blob=subprocess.check_output([GIT,'archive','--format=zip',FROZEN,'port/matrixgame'])
    with zipfile.ZipFile(io.BytesIO(blob)) as archive:
        assert all(not Path(n).is_absolute() and '..' not in Path(n).parts and n.startswith('port/') for n in archive.namelist())
        archive.extractall(tree)
    frozen=tree/'port/matrixgame/native-engine/native_device_state.cpp'
    test='tests/benchmark_matrixgame_state_ctor.cpp' if kind=='benchmark' else 'tests/test_matrixgame_state_ctor.cpp'
    common=['port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp','tests/matrixgame_stencil_validate_probe.cpp'];current='port/matrixgame/native-engine/native_device_state.cpp'
    if kind!='benchmark':common+=['tests/matrixgame_state_ctor_allocations.cpp']
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-2500:] if p.returncode or label.startswith('runtime') else '',flush=True);assert p.returncode==0,label;return log
    try:
        programs={};measurements=[]
        for variant in ('frozen','candidate'):
            base=tree if variant=='frozen' else ROOT;includes=[base/'port/matrixgame'/n for n in ('native-engine','native-core','deko')];files=[ROOT/test,base/current,*[(ROOT if p.startswith('tests/') else base)/p for p in common]];exe=out/variant
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
                log=run(programs[variant],'runtime-'+variant+'-'+str(i));m={k:int(v) for k,v in re.findall(r'(set_us|get_us|raster_us|prepare_us|validate_us|ctor_us|checksum)=(\d+)',log)};measurements.append({'variant':variant,**m})
            assert len({m['checksum'] for m in measurements})==1
            j['measurements']=measurements;j['median_us']={v:{k:statistics.median(m[k] for m in measurements if m['variant']==v) for k in ('set_us','get_us','raster_us','prepare_us','validate_us','ctor_us')} for v in programs}
        else:
            outputs={v:run(cmd,'runtime-'+v) for v,cmd in programs.items()};assert outputs['candidate']==outputs['frozen'],'frozen behavior differs'
            assert 'STATE_CTOR' in outputs['candidate'];j['semantic_stdout']=outputs['candidate']
        j['status']='PASS FROZEN R32/CANDIDATE CPU O2 COMPARISON; GUARDS RETAINED; HARDWARE_PENDING'
    finally:
        paths=[test,current,*common,'port/matrixgame/native-engine/native_device_state.hpp','port/matrixgame/native-engine/native_state_layout.hpp','port/matrixgame/deko/stencil_state.hpp','tests/matrixgame_state_ctor_probe.hpp','tools/matrixgame_state_ctor_checks.py'];j['source_sha256']={p:sha(ROOT/p) for p in paths};j['frozen_commit']=FROZEN;j['frozen_cpp_sha256']=sha(frozen);j['frozen_stencil_sha256']=sha(tree/'port/matrixgame/deko/stencil_state.hpp');(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

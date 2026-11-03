"""Compare frozen r17 and current state CPP at O2: semantics and CPU timings."""
import argparse,hashlib,json,re,subprocess,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GIT='C:/Program Files/Git/cmd/git.exe';FROZEN='0d18392f001a8566f8f676bb4220c0a4e25e3d53'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows','benchmark'));ap.add_argument('--ownership',action='store_true');args=ap.parse_args();kind=args.kind;assert not (args.ownership and kind=='benchmark')
    parent=ROOT/'build/matrixgame/e610-coverage-r18-checks'/(kind+('-ownership' if args.ownership else ''));parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();j={'status':'INCOMPLETE','commands':[],'scope':'real frozen/candidate CPU validator; no Switch/GPU/FPS'}
    frozen=out/'frozen_native_device_state.cpp';frozen.write_bytes(subprocess.check_output([GIT,'show',FROZEN+':port/matrixgame/native-engine/native_device_state.cpp']))
    frozen_vertex=out/'frozen_fixed_vertex.cpp';frozen_vertex.write_bytes(subprocess.check_output([GIT,'show',FROZEN+':port/matrixgame/deko/fixed_vertex.cpp']))
    def extract(source,signature):
        a=source.index(signature);b=source.index('\n}\n',a)+2;return source[a:b]+'\n'
    platform='port/matrixgame/native-engine/platform_switch.cpp'
    original=subprocess.check_output([GIT,'show',FROZEN+':'+platform]).decode().replace('\r\n','\n');candidate=(ROOT/platform).read_text(encoding='utf-8')
    signature='DeviceResult DekoDevice::NativeDrawIndexed('
    old=extract(original,signature);new=extract(candidate,signature)
    expected=old
    assert new==expected,'indexed body changed beyond direct decode destination'
    (out/'gather.inc').write_text('#ifdef DIRECT_DECODE\n'+new.replace('DekoDevice::NativeDrawIndexed','IndexedHarness::Gather')+'#else\n'+old.replace('DekoDevice::NativeDrawIndexed','IndexedHarness::Gather')+'#endif\n')
    a=candidate.index('DkPrimitive NativePrimitive(');b=candidate.index('\n}',a)
    (out/'native-primitive.inc').write_text(candidate[a:b]+'\n')
    frozen_staging=subprocess.check_output([GIT,'show',FROZEN+':port/matrixgame/native-engine/staging_draw.hpp'])
    (out/'frozen-staging.hpp').write_bytes(frozen_staging)
    (out/'candidate-staging.hpp').write_bytes((ROOT/'port/matrixgame/native-engine/staging_draw.hpp').read_bytes())
    (out/'staging_draw.hpp').write_text('#ifdef COVERAGE_VIEW\n#include "candidate-staging.hpp"\n#else\n#include "frozen-staging.hpp"\n#endif\n')
    counted=out/'counted-buffer.cpp'
    resource=(ROOT/'port/matrixgame/native-core/buffer_resources.cpp').read_text(encoding='utf-8')
    signature='bool Resource::Initialized(std::size_t offset,std::size_t bytes) const noexcept {'
    assert resource.count(signature)==1
    counted.write_text(resource.replace('namespace srhd_awa::matrixgame::buffers {','std::uint64_t matrixgame_initialized_calls{};\nnamespace srhd_awa::matrixgame::buffers {').replace(signature,signature+'\n    ++::matrixgame_initialized_calls;'))
    test='tests/benchmark_matrixgame_decode_target.cpp' if kind=='benchmark' else 'tests/test_matrixgame_native_state.cpp' if args.ownership else 'tests/test_matrixgame_coverage.cpp'
    common=['port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/material_combiner.cpp'];current='port/matrixgame/native-engine/native_device_state.cpp'
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-2500:] if p.returncode or label.startswith('runtime') else '',flush=True);assert p.returncode==0,label;return log
    try:
        programs={};measurements=[];includes=[out]+[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')]+[Path('C:/devkitPro/libnx/include')]
        for variant in ('frozen','candidate'):
            defines=['-DDIRECT_DECODE']+(['-DCOVERAGE_VIEW'] if variant=='candidate' else [])+(['-DBENCHMARK_DECODE'] if kind=='benchmark' else [])
            files=[ROOT/test,frozen if variant=='frozen' else ROOT/current,*[ROOT/p for p in common],frozen_vertex if variant=='frozen' else ROOT/'port/matrixgame/deko/fixed_vertex.cpp'];files=[counted if kind!='benchmark' and p==ROOT/'port/matrixgame/native-core/buffer_resources.cpp' else p for p in files];exe=out/variant
            if kind=='windows':
                obj=out/(variant+'-objects');obj.mkdir();vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
                cmd=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG /fp:strict '+' /DDIRECT_DECODE '+(' /DCOVERAGE_VIEW ' if variant=='candidate' else '')+(' /DBENCHMARK_DECODE ' if kind=='benchmark' else '')+' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(p)+'"' for p in files)+f' /Fo"{obj.as_posix()}/" /Fe"{exe}.exe"'
                script=out/(variant+'.cmd');script.write_text(cmd+'\n');run(['cmd.exe','/d','/c',script],'build-'+variant);programs[variant]=[str(exe)+'.exe']
            else:
                flags=['-std=c++20','-O2','-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie','-fno-omit-frame-pointer']
                if kind=='host':flags+=['-fsanitize=address,undefined']
                if kind=='arm64':
                    version=out/'local-typeinfo.map';version.write_text('{ local: _ZNKSt9type_infoeqERKS_; };\n');flags+=['-fsanitize=undefined','-Wl,--version-script='+linux(version)]
                run(['wsl','--exec','aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++',*flags,*defines,*['-I'+linux(p) for p in includes],*map(linux,files),'-o',linux(exe)],'build-'+variant)
                runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
                if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
                programs[variant]=runtime+[linux(exe)]
        if kind=='benchmark':
            for i,variant in enumerate(('frozen','candidate','candidate','frozen','frozen','candidate')):
                log=run(programs[variant],'runtime-'+variant+'-'+str(i));m={k:int(v) for k,v in re.findall(r'(decode_us|gather_us|checksum)=(\d+)',log)};measurements.append({'variant':variant,**m})
            assert len({m['checksum'] for m in measurements})==1
            j['measurements']=measurements;j['median_us']={v:{k:statistics.median(m[k] for m in measurements if m['variant']==v) for k in ('decode_us','gather_us')} for v in programs}
        else:
            outputs={v:run(cmd,'runtime-'+v) for v,cmd in programs.items()};assert outputs['candidate']==outputs['frozen'],'frozen behavior differs'
            assert ('restores=9 capture_OOM=2 COW_OOM=1' if args.ownership else 'COVERAGE_VALIDATE') in outputs['candidate'];j['semantic_stdout']=outputs['candidate']
        j['status']='PASS FROZEN R17/CANDIDATE CPU O2 COMPARISON; GUARDS RETAINED; HARDWARE_PENDING'
    finally:
        paths=[test,'port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/quantized_argb.hpp','tests/matrixgame_decode_target_fixture.hpp','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/deko/fixed_vertex.hpp','port/matrixgame/native-engine/native_profile.hpp','port/matrixgame/native-engine/staging_draw.hpp',current,*common,'port/matrixgame/native-engine/native_device_state.hpp','port/matrixgame/native-engine/native_state_layout.hpp','tools/matrixgame_coverage_checks.py'];j['source_sha256']={p:sha(ROOT/p) for p in paths};j['extracted_inputs_sha256']={p.name:sha(p) for p in out.glob('*.inc')};j['frozen_staging_sha256']=sha(out/'frozen-staging.hpp');j['test_staging_sha256']={p.name:sha(p) for p in out.glob('*staging*')};j['initialization_counter']={'enabled':kind!='benchmark','compiled_source_sha256':sha(counted),'production_source_sha256':sha(ROOT/'port/matrixgame/native-core/buffer_resources.cpp'),'scope':'exact original body with a single CPU-only counter; timing uses uninstrumented production CPP'};j['frozen_commit']=FROZEN;j['frozen_cpp_sha256']=sha(frozen);j['frozen_vertex_cpp_sha256']=sha(frozen_vertex);(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

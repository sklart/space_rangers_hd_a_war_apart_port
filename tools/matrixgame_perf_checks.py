"""Targeted r8 recycler, packet, journal and Init-handoff checks; no battle rerun."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows'));ap.add_argument('--optimization',choices=('O1','O2'),default='O1');ap.add_argument('--output-root',type=Path,default=ROOT/'build/matrixgame/e610-perf-r8-checks');args=ap.parse_args();kind=args.kind
    parent=args.output_root.resolve()/kind;assert parent.is_relative_to(ROOT/'build');parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();result={'status':'INCOMPLETE','commands':[],'scope':'CPU ownership/data/real file IO; no Switch GPU, no battle rerun'}
    source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text(encoding='utf-8')
    def body(start):
        a=source.index('{',source.index(start));b=a+1;depth=1
        while depth:
            if source[b]=='{':depth+=1
            elif source[b]=='}':depth-=1
            b+=1
        return source[a:b]
    for name,signature in [('flush-recording','    void FlushRecording()'),('complete-init','void DekoDevice::CompleteNativeInitializationWork()')]:
        (out/(name+'.inc')).write_text(('void FlushRecording()' if name=='flush-recording' else 'void CompleteNativeInitializationWork()')+body(signature)+'\n',encoding='utf-8')
    (out/'allocation-size.inc').write_text('std::uint32_t AllocationSize(std::uint64_t bytes)'+body('std::uint32_t AllocationSize(')+'\n',encoding='utf-8')
    baseline_upload=ROOT/'build/matrixgame/e2e554-draw-upload/baseline-upload.inc';(out/'baseline-upload.inc').write_bytes(baseline_upload.read_bytes())
    baseline_pool=subprocess.check_output(['C:/Program Files/Git/cmd/git.exe','show','dd7f6714872d1826bc35f896ee4975bc104f9ea8:port/matrixgame/native-engine/completed_block_pool.hpp'])
    (out/'baseline-exact-pool.inc').write_bytes(baseline_pool.replace(b'CompletedBlockPool',b'FrozenExactPool'))
    common=['port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
    cases={'native_state':common, 'completed_best_fit':[], 'draw_upload':[], 'completed_pool':[], 'material_uniform_packet':[], 'native_draw_batch':[], 'init_handoff':common,'diagnostic_journal':['port/matrixgame/native-engine/native_diagnostic_journal.cpp']}
    captures=[ROOT/'build/matrixgame/e2e539-batches/baseline-windows'/('frame-cost-'+str(i)+'.csv') for i in range(2)]
    paths=set()
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-2600:] if p.returncode or label.endswith('runtime') else '',flush=True);assert p.returncode==0,label;return log
    try:
        for name,extra in cases.items():
            files=['tests/test_matrixgame_'+name+'.cpp',*extra];paths.update(files);includes=[out,*[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')],Path('C:/devkitPro/libnx/include')];exe=out/name
            if kind=='windows':
                vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
                cmd=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG '+' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(ROOT/p)+'"' for p in files)+f' /Fo"{out.as_posix()}/" /Fe"{exe}.exe"'
                script=out/(name+'.cmd');script.write_text(cmd+'\n');run(['cmd.exe','/d','/c',script],name+'-build');runtime=[str(exe)+'.exe']
            else:
                flags=['-std=c++20','-'+args.optimization,'-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined' if kind=='host' else '-fsanitize=undefined']
                if kind=='arm64':
                    version=out/'local-typeinfo.map';version.write_text('{ local: _ZNKSt9type_infoeqERKS_; };\n');flags+=['-Wl,--version-script='+linux(version)]
                run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in files],'-o',linux(exe)],name+'-build')
                runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
                if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
                runtime+=[linux(exe)]
            if name in ('completed_pool','native_draw_batch'):runtime+=[str(p) if kind=='windows' else linux(p) for p in captures]
            if name=='diagnostic_journal':runtime+=[str(out/'filesystem') if kind=='windows' else linux(out/'filesystem')]
            result[name+'_stdout']=run(runtime,name+'-runtime')
        result['status']='PASS BOUNDED BEST FIT/CACHED SIZE/PHYSICAL QUOTA/EXACT UPLOAD/PACKET/BATCH ORDER/INIT HANDOFF/JOURNAL FAILURES; CPU ONLY'
    finally:
        paths.update(['port/matrixgame/native-engine/'+n for n in ('completed_block_pool.hpp','native_upload_reuse.hpp','native_profile.hpp','native_draw_batch.hpp','native_frame_scope.hpp','native_draw_upload.hpp','platform_switch.cpp','platform_switch.hpp','native_diagnostic_journal.hpp')])
        paths.update(['port/matrixgame/deko/material_program.cpp','port/matrixgame/deko/material_program.hpp','port/matrixgame/deko/material_uniform_packet.hpp','tools/matrixgame_perf_checks.py','tests/matrixgame_native_diagnostic_main.cpp'])
        result['source_sha256']={p:sha(ROOT/p) for p in sorted(paths)};result['captures_sha256']={str(p.relative_to(ROOT)):sha(p) for p in captures};result['test_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.glob('*.inc')};result['baseline_upload_sha256']=sha(baseline_upload);(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

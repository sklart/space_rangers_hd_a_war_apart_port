"""Fresh targeted managed-residency tests; real codecs, CPU upload model only."""
import argparse,hashlib,json,os,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows','baseline'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-managed-r4-checks'/kind;parent.mkdir(parents=True,exist_ok=True);index=1
    while (parent/str(index)).exists():index+=1
    out=parent/str(index);out.mkdir();result={'status':'INCOMPLETE','commands':[],'scope':'real asset decode/mips/CPU copies plus CPU residency model; NO GPU or full Init'}
    inputs=read(ROOT/'build/matrixgame/e2e521/residency/host/result.json')['inputs']
    files=[ROOT/p for p in inputs]
    for p in files:assert sha(p)==inputs[str(p.relative_to(ROOT))]
    result['inputs_sha256']=inputs
    def run(cmd,label,expected_failure=False):
        p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
        log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8')
        result['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'expected_failure':expected_failure})
        print(kind,label,p.returncode,log[-3000:] if p.returncode or label=='runtime' else '',flush=True)
        assert (p.returncode!=0 if expected_failure else p.returncode==0),label
        return log
    try:
        if kind=='windows':
            old=read(ROOT/'build/matrixgame/e2e521/residency/windows/result.json');objects=[]
            for name in ('native_texture','platform_3g','test_matrixgame_texture_residency'):
                command=next(c['command'] for c in old['commands'] if c['label']==name).replace('\\\\','\\')
                command=command.replace(str(ROOT/'build/matrixgame/e2e521/residency/windows'),str(out))
                script=out/(name+'.cmd');script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],name)
            command=next(c['command'] for c in old['commands'] if c['label']=='native_texture').replace('\\\\','\\')
            command=command.replace(str(ROOT/'port/matrixgame/native-engine/native_texture.cpp'),str(ROOT/'port/matrixgame/native-core/texture_mips.cpp'))
            command=command.replace(str(ROOT/'build/matrixgame/e2e521/residency/windows/native_texture.obj'),str(out/'texture_mips.obj'))
            script=out/'texture_mips.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'texture_mips')
            command=next(c['command'] for c in old['commands'] if c['label']=='link').replace('\\\\','\\').replace(str(ROOT/'build/matrixgame/e2e521/residency/windows'),str(out))
            command=command.replace(' kernel32.lib',' "'+str(out/'texture_mips.obj')+'" kernel32.lib')
            script=out/'link.cmd';script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],'link')
            result['stdout']=run([out/'texture-residency.exe',*files],'runtime')
        else:
            target_kind='host' if kind=='baseline' else kind
            old=read(ROOT/f'build/matrixgame/e2e521/residency/{target_kind}/result.json')
            command=next(c['command'][:] for c in old['commands'] if c['label']=='build-link')
            command[command.index('-o')+1]=linux(out/'texture-residency')
            first_archive=next(i for i,c in enumerate(command) if c.endswith('.a'))
            command.insert(first_archive,linux(ROOT/'port/matrixgame/native-core/texture_mips.cpp'))
            if kind=='arm64':command+=['-fsanitize=undefined','-fno-sanitize-recover=undefined','-fvisibility-inlines-hidden']
            if kind=='baseline':
                data=subprocess.check_output(['C:/Program Files/Git/cmd/git.exe','show','d09b7859e693bc6d33d0c8328ecd5bbc50839c52:port/matrixgame/native-engine/native_texture.cpp'])
                previous=out/'r3-native_texture.cpp';previous.write_bytes(data)
                command=[linux(previous) if c==linux(ROOT/'port/matrixgame/native-engine/native_texture.cpp') else c for c in command]
                result['baseline_source_sha256']=sha(previous)
            if kind=='arm64':
                # Keep every UBSan check, including vptr. Do not export the
                # instrumented std::type_info inline into libubsan/libstdc++:
                # that interposition recurses through checkDynamicType.
                sources=[s for s in command if s.endswith('.cpp')]
                first_source=min(command.index(s) for s in sources)
                flags=[s for s in command[3:first_source] if s not in ('-Wl,--gc-sections','-no-pie')]
                sanitize=['-fsanitize=undefined','-fno-sanitize-recover=undefined','-fvisibility-inlines-hidden']
                def compile_source(source):
                    obj=out/(Path(source).stem+'.o')
                    run([*command[:3],*flags,*sanitize,'-c',source,'-o',linux(obj)],'compile-'+obj.stem)
                    return linux(obj)
                with ThreadPoolExecutor(max_workers=3) as pool:objects=list(pool.map(compile_source,sources))
                first_archive=next(i for i,s in enumerate(command) if s.endswith('.a'))
                libs=command[first_archive:command.index('-o')]
                run([*command[:3],'-Wl,--gc-sections','-no-pie',*sanitize,*objects,*libs,'-o',linux(out/'texture-residency')],'link')
            else:run(command,'build-link')
            runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            result['stdout']=run([*runtime,linux(out/'texture-residency'),*map(linux,files)],'runtime',kind=='baseline')
            if kind=='baseline':assert 'ResidencyExhausted' in result['stdout']
        result['source_sha256']={p:sha(ROOT/p) for p in ('port/matrixgame/native-engine/native_texture.cpp','port/matrixgame/native-engine/native_texture.hpp','port/matrixgame/native-core/texture_mips.cpp','tests/test_matrixgame_texture_residency.cpp','tools/matrixgame_managed_pressure_checks.py')}
        result['status']='PASS OLD CODE REPRODUCES UNHANDLED ARENA EXHAUSTION' if kind=='baseline' else 'PASS MANAGED EVICTION/RESTORE/PINS/PRIORITY/REAL ASSET BYTES; CPU MODEL ONLY'
    finally:
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('result',out/'result.json',flush=True)
if __name__=='__main__':main()

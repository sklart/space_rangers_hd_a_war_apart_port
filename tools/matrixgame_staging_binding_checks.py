"""Targeted real staging/setter/decode/state-lease checks; no GPU or Init success."""
import argparse,hashlib,json,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp',
         'port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp','tests/test_matrixgame_staging_binding.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def body(text,marker):
    start=text.index(marker);brace=text.index('{',start);i=brace+1;depth=1
    while depth:
        depth+=(text[i]=='{')-(text[i]=='}');i+=1
    return text[brace:i]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('windows','host','arm64','baseline'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-staging-r5-checks'/kind;parent.mkdir(parents=True,exist_ok=True);index=1
    while (parent/str(index)).exists():index+=1
    out=parent/str(index);out.mkdir();j={'status':'INCOMPLETE','commands':[],'scope':'actual buffers/fixed vertex decoding/state snapshots; NO GPU or original Init'}
    includes=[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')]+[out]
    sources=[ROOT/p for p in SOURCES]
    def run(cmd,label,negative=False):
        p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
        log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'expected_failure':negative})
        print(kind,label,p.returncode,log[-2500:] if p.returncode or label=='runtime' else '',flush=True)
        assert (p.returncode!=0 if negative else p.returncode==0),label
        return log
    try:
        if kind=='baseline':
            original=subprocess.check_output(['C:/Program Files/Git/cmd/git.exe','show','d5af0be47d5d48b8d8f392c403272cb6f9486026:port/matrixgame/native-engine/platform_switch.cpp']).decode('utf-8')
            text='DeviceResult OldVertex(std::uint32_t stream,buffers::VertexBuffer* buffer,std::uint32_t offset,std::uint32_t stride)'+body(original,'DeviceResult DekoDevice::BindStagingVertex(')
            text+='\nDeviceResult OldIndex(buffers::IndexBuffer* buffer)'+body(original,'DeviceResult DekoDevice::BindStagingIndex(')
            (out/'r4-binding.inc').write_text(text,encoding='utf-8');j['baseline_extracted_sha256']=sha(out/'r4-binding.inc')
        if kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            cmd=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /Gy /Gw /UNDEBUG '
            cmd+=' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(p)+'"' for p in sources)
            cmd+=' /Fo"'+out.as_posix()+'/" /Fe"'+str(out/'binding.exe')+'" /link /OPT:REF'
            script=out/'build.cmd';script.write_text(cmd+'\n');run(['cmd.exe','/d','/c',script],'build')
            j['stdout']=run([out/'binding.exe'],'runtime')
        else:
            compiler='aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++'
            flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffunction-sections','-fdata-sections','-fno-pie','-ffp-contract=off']+['-I'+linux(p) for p in includes]
            if kind=='arm64':flags+=['-fsanitize=undefined','-fno-sanitize-recover=undefined','-fvisibility-inlines-hidden']
            else:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            if kind=='baseline':flags+=['-DMATRIXGAME_BINDING_BASELINE=1']
            def compile_one(p):
                obj=out/(p.stem+'.o');run(['wsl','--exec',compiler,*flags,'-c',linux(p),'-o',linux(obj)],'compile-'+p.stem);return linux(obj)
            with ThreadPoolExecutor(max_workers=3) as pool:objects=list(pool.map(compile_one,sources))
            link=['wsl','--exec',compiler,*flags,'-no-pie','-Wl,--gc-sections',*objects,'-o',linux(out/'binding')]
            if kind=='arm64':
                visibility=out/'ubsan-local.map';visibility.write_text('{ local: _ZNKSt9type_infoeqERKS_; };\n');link+=['-Wl,--version-script='+linux(visibility)]
            run(link,'link');runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            j['stdout']=run([*runtime,linux(out/'binding')],'runtime',kind=='baseline')
            if kind=='baseline':assert 'native vertex binding differs from retained staging context' in j['stdout']
        tracked=SOURCES+['port/matrixgame/native-engine/native_staging_binding.hpp','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-core/buffer_resources.hpp','tools/matrixgame_staging_binding_checks.py']
        j['source_sha256']={p:sha(ROOT/p) for p in tracked};j['status']='PASS R4 BINDING FAILURE REPRODUCED' if kind=='baseline' else 'PASS REAL STAGING SETTERS/DECODE/LEASE/LIFETIME/INVALID INPUTS; NO GPU'
    finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8');print('result',out/'result.json',flush=True)
if __name__=='__main__':main()

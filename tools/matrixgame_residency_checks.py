"""Focused real texture resource/SDK checks. Never writes an NRO or resources."""
import argparse,hashlib,json,mmap,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/residency'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','sdk'));args=ap.parse_args();kind=args.kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True)
    evidence={'status':'INCOMPLETE','commands':[],'Switch_runtime':False,'NRO_created':False}
    def run(cmd,label,cwd=ROOT,escalate=False):
        p=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'))
        evidence['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode});(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        print(kind,label,p.returncode,(p.stdout+p.stderr)[-6500:] if p.returncode or label=='runtime' else '',flush=True)
        if p.returncode:raise RuntimeError(label)
        return p.stdout
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    sources=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_texture.cpp','legacy_texture_backend.cpp','unmanaged_texture_backend.cpp','platform_3g.cpp')]
    try:
        prior=json.loads((ROOT/f'build/matrixgame/e2e521/{kind}/result.json').read_text())
        if kind in ('host','arm64'):
            sys.path.insert(0,str(ROOT/'tests'));from probe_ui_images_release import entries,payload
            from probe_matrixgame_native_texture import dds,storage
            controls=out/'inputs';controls.mkdir(exist_ok=True);files=[]
            for fmt in ('rgba','bc1','bc2','bc3'):
                path=controls/(fmt+'.dds');path.write_bytes(dds(fmt,8,8,4)[0] if isinstance(dds(fmt,8,8,4),tuple) else dds(fmt,8,8,4));files.append(path)
            package=ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg'
            with package.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
                chosen={}
                for entry in entries(data):
                    ext=Path(entry[0]).suffix.lower()
                    if ext not in ('.dds','.png','.jpg','.strg') or ext in chosen:continue
                    b=payload(data,entry)
                    if len(b)>1024*1024:continue
                    path=controls/('original'+ext);path.write_bytes(b);files.append(path);chosen[ext]={'path':entry[0],'sha256':hashlib.sha256(b).hexdigest()}
                    if len(chosen)==4:break
                evidence['original_inputs']=chosen
            evidence['package_sha256']=hashlib.sha256(package.read_bytes()).hexdigest()
            prefix=['wsl','--exec'];template=next(c.copy() for c in prior['commands'] if '-c' in c)
            sources+=[ROOT/'tests/test_matrixgame_texture_residency.cpp']
            cmd=template.copy();i=cmd.index('-c');del cmd[i:i+2];i=cmd.index('-o');del cmd[i:i+2]
            cmd=[x for x in cmd if x not in ('-O3','-DNDEBUG')]+['-O1','-g','-fno-pie']
            target=out/'texture-residency';oldlink=next(c for c in prior['commands'] if '-Wl,--gc-sections' in c)
            # Retain only the genuine CPU core archive/codec dependencies.
            legacy=ROOT/f'build/matrixgame/e2e519/vector-group-fresh-{kind}'
            libs=oldlink[oldlink.index('core/libmatrixgame-native-core.a'):]
            libs=[linux(legacy/x) if x=='core/libmatrixgame-native-core.a' else x for x in libs]
            run([*prefix,*cmd,'-Wl,--gc-sections','-no-pie',*map(linux,sources),*libs,'-o',linux(target)],'build-link')
            cmd=[*prefix,'env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            evidence['stdout']=run([*cmd,linux(target),*map(linux,files)],'runtime')
            evidence['inputs']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
        else:
            template=next(c.copy() for c in prior['commands'] if '-c' in c);objects=[]
            sources+=[ROOT/'port/matrixgame/deko/texture_store.cpp',ROOT/'port/matrixgame/native-engine/platform_switch.cpp',ROOT/'tests/probe_matrixgame_native_entry.cpp']
            for source in sources:
                cmd=template.copy();obj=out/(source.stem+'.o');cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj)
                cmd+=['-IC:/devkitPro/libnx/include','-I'+str(ROOT/'build/matrixgame/e2e521/cascade/sdk')]
                run(cmd,source.stem);objects.append(obj)
            old=json.loads((ROOT/'build/matrixgame/e2e521/cascade/native-entry/result.json').read_text())
            link=next(c['command'].copy() for c in old['commands'] if c['label']=='link')
            link=[s for s in link if s.replace('\\','/').rsplit('/',1)[-1] not in ('platform_switch.o','driver.o','platform.o')]
            first=next(i for i,s in enumerate(link) if s.endswith('.a'));link[first:first]=list(map(str,objects))
            link[link.index('-o')+1]=str(out/'native-entry.elf');run(link,'link')
            evidence['link_inputs_sha256']={str(Path(s)):hashlib.sha256(Path(s).read_bytes()).hexdigest() for s in link if Path(s).suffix in ('.o','.a') and Path(s).is_file()}
            nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',out/'native-entry.elf'],'symbols')
            evidence['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not evidence['strong_undefined']
            evidence['required_symbols']={}
            for symbol in ('CGame::Init(','DekoDevice::AvailableTextureBytes()','DekoTextureStore::Upload(','native_texture::Decode(','dkCmdBufCopyBufferToImage','DekoDevice::BindBorrowedTexture('):
                found=[s for s in nm.splitlines() if symbol in s];assert found,symbol;evidence['required_symbols'][symbol]=found[:3]
            evidence['elf_sha256']=hashlib.sha256((out/'native-entry.elf').read_bytes()).hexdigest()
        headers=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_texture.hpp','texture_residency.hpp','platform_switch.hpp')]
        evidence['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+headers+[Path(__file__).resolve(),ROOT/'port/matrixgame/deko/texture_store.hpp']}
        evidence['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

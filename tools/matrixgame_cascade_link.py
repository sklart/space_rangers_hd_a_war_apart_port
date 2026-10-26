"""Link current native providers/materials to the retained original Init/frame."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521'
def main():
    out=BASE/'cascade/native-entry';out.mkdir(parents=True,exist_ok=True)
    prior=json.loads((BASE/'sdk/result.json').read_text());report={'commands':[],'runtime_executed':False,'status':'INCOMPLETE'}
    def run(command,label):
        p=subprocess.run(command,cwd=ROOT,env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'),capture_output=True,text=True)
        (out/(label+'.log')).write_text(p.stdout+p.stderr);report['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(label,p.returncode,(p.stdout+p.stderr)[-5000:] if p.returncode else '',flush=True)
        if p.returncode:raise RuntimeError(label)
        return p.stdout
    try:
        template=next(c for c in prior['commands'] if '-c' in c)
        objects={}
        for label,name in {'driver':'tests/probe_matrixgame_native_entry.cpp','map-device':'port/matrixgame/native-engine/map_device_backend.cpp'}.items():
            cmd=template.copy();cmd[cmd.index('-c')+1]=str(ROOT/name);obj=out/(label+'.o');cmd[cmd.index('-o')+1]=str(obj)
            cmd+=['-IC:/devkitPro/libnx/include','-I'+str(BASE/'cascade/sdk')]
            run(cmd,label);objects[label]=str(obj)
        link=next(c.copy() for c in prior['commands'] if '-o' in c and c[c.index('-o')+1].endswith('native-entry.elf'))
        link=[str(BASE/'cascade/sdk/platform_switch.o') if x==str(BASE/'sdk/switch.o') else objects['driver'] if x==str(BASE/'sdk/driver.o') else x for x in link]
        link[link.index('-o')+1]=str(out/'native-entry.elf')
        first_library=next(i for i,x in enumerate(link) if x.endswith('.a'))
        link[first_library:first_library]=[objects['map-device'],*[str(BASE/'cascade/sdk'/f'{name}.o') for name in ('material_combiner','material_program','fixed_vertex')]]
        run(link,'link')
        nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',str(out/'native-entry.elf')],'symbols')
        report['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not report['strong_undefined']
        report['required_symbols']={}
        for symbol in ('CGame::Init(','CMatrixMap::Draw()','DekoDevice::ReadCaps()','DekoDevice::NativeDrawIndexed(',
                       'FixedVertexState::Prepare()','dkShaderInitialize','dkCmdBufDrawIndexed','dkQueueAcquireImage','dkQueuePresentImage'):
            matches=[s for s in nm.splitlines() if symbol in s];assert matches,symbol;report['required_symbols'][symbol]=matches[:3]
        report['inputs_sha256']={}
        for c in report['commands']:
            for item in c['command']:
                p=Path(item)
                if p.is_file() and p.suffix in ('.a','.o','.cpp'):report['inputs_sha256'][str(p)]=hashlib.sha256(p.read_bytes()).hexdigest()
        report['elf_sha256']=hashlib.sha256((out/'native-entry.elf').read_bytes()).hexdigest();report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()

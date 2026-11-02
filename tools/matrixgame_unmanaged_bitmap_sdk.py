"""Build a separate diagnostic NRO with the actual current SDK target."""
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'build/matrixgame/e2e561-unmanaged-bitmap/sdk'


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    BASE.mkdir(parents=True,exist_ok=True)
    report={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN'}
    prior=json.loads((ROOT/'build/matrixgame/e2e558-gamma/sdk/result.json').read_text())
    cmd=prior['commands'][0]['command'][:]
    cmd[cmd.index('-B')+1]=str(BASE)
    env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
    def run(command,label):
        command=list(map(str,command));start=time.monotonic()
        p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
        log=p.stdout+p.stderr;(BASE/(label+'.log')).write_text(log,encoding='utf-8')
        report['commands'].append({'label':label,'command':command,'exit':p.returncode,'seconds':time.monotonic()-start})
        print(label,p.returncode,log[-5000:] if p.returncode else '',flush=True)
        assert p.returncode==0,label
        return log
    protected=[ROOT/'build/matrixgame/e2e558-gamma/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro',ROOT/'build/matrixgame/e2e556-standard-nro/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro',Path('D:/logs2.7z')]
    protected += [p for p in (ROOT/'dist').rglob('*.nro')]
    before={str(p):sha(p) for p in protected}
    report['protected_before']=before
    try:
        run(cmd,'configure')
        run([cmd[0],'--build',BASE,'--target','matrixgame-native-diagnostic-nro','-j','4'],'build')
        elf=BASE/'matrixgame-native-diagnostic.elf'
        nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
        undefined=[line for line in nm.splitlines() if ' U ' in line]
        assert not undefined,undefined
        assert 'CGame::Init(' in nm
        archive=BASE/'libmatrixgame-native-cache.a'
        archive_symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',archive],'archive-symbols')
        for name in ('native_texture::DecodeBitmap(', 'CTexture::LoadFromBitmap('):assert name in archive_symbols,name
        # The one-argument loader is a real ported dependency, but is currently
        # unused by the diagnostic's reachable Init graph and can be discarded.
        report['bitmap_decoder_reachable_in_elf']='native_texture::DecodeBitmap(' in nm
        report['bitmap_decoder_defined_in_archive']=True
        report['strong_undefined']=undefined
        nro=BASE/'diagnostic-nro/matrixgame-native-diagnostic.nro'
        report['artifacts']={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (elf,nro)}
        report['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.a','.obj','.o')}
        report['status']='PASS; SDK COMPILE LINK NRO ONLY'
    finally:
        report['protected_after']={str(p):sha(p) for p in protected}
        assert before==report['protected_after']
        (BASE/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()

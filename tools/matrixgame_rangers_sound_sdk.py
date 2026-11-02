"""SDK archive/new-host-symbol compile and existing bridge link regression.

The probe does not bind real Rangers services on Switch and produces no NRO.
"""
import json
import os
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
BASE=ROOT/'build/matrixgame/e2e569-rangers-sound/sdk'
def main():
    BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','NRO':'NOT BUILT; latest diagnostic E568 preserved'}
    command=read(ROOT/'build/matrixgame/e2e568-native-services/sdk/result.json')['commands'][0]['command'][:]
    command[command.index('-S')+1]=str(ROOT/'port/matrixgame/native');command[command.index('-B')+1]=str(BASE);command+=['-DBRIDGE_BUILD_LINK_PROBE=ON']
    env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (BASE/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode})
        print(label,p.returncode,log[-5000:] if p.returncode else '',flush=True);assert p.returncode==0,label
        return log
    names=['port/matrixgame/native/'+name for name in ('CMakeLists.txt','rangers_host_callbacks.cpp','rangers_host_callbacks.hpp','rangers_audio_session.cpp','rangers_audio_session.hpp','rangers_bridge.cpp','engine_callbacks.cpp')]
    before={name:sha(ROOT/name) for name in names}
    try:
        run(command,'configure');run([command[0],'--build',BASE,'--target','matrixgame-engine-api-link-probe','-j','4'],'build')
        nm='C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe';archive=BASE/'libmatrixgame-engine-callbacks.a';elf=BASE/'matrixgame-engine-api-link-probe.elf'
        symbols=run([nm,'-C',archive],'archive-symbols')
        for name in ('MakeRangersHostCallbacks(', 'RangersAudioSession::RangersAudioSession('):assert name in symbols,name
        linked=run([nm,'-C',elf],'symbols');undefined=[line for line in linked.splitlines() if ' U ' in line];assert not undefined,undefined
        result['strong_undefined']=undefined;result['new_host_code_reachable_in_probe']='MakeRangersHostCallbacks(' in linked or 'RangersAudioSession::RangersAudioSession(' in linked
        assert not result['new_host_code_reachable_in_probe']
        result['artifacts']={str(path.relative_to(ROOT)):{'sha256':sha(path),'bytes':path.stat().st_size} for path in (archive,elf)}
        result['link_inputs_sha256']={str(path.relative_to(ROOT)):sha(path) for path in BASE.rglob('*') if path.is_file() and path.suffix in ('.a','.obj','.o')}
        assert before=={name:sha(ROOT/name) for name in names}
        result['status']='PASS; SDK NEW HOST ARCHIVE COMPILE; EXISTING BRIDGE LINK ONLY'
    finally:
        result['source_sha256']=before;(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

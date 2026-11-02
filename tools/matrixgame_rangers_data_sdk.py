"""Full SDK link with the real shared data owner and borrowed audio/music scopes."""
import json
import os
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from e2e_dat_cleanup import apply as dat_cleanup
BASE=ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final'

def main():
    BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','new_NRO':False}
    prior=read(ROOT/'build/matrixgame/e2e572-rangers-host-sdk/result.json')
    check_hashes(prior['archive_objects_sha256']);check_hashes(prior['dependency_sha256'])
    protected={name:value['sha256'] for name,value in prior['artifacts'].items()};check_hashes(protected)
    sources=[ROOT/'port/matrixgame/native'/name for name in ('CMakeLists.txt','engine_callbacks.cpp','engine_callbacks.hpp','rangers_bridge.cpp','rangers_host_callbacks.cpp','rangers_audio_session.cpp','rangers_audio_session.hpp','rangers_music_session.cpp','rangers_music_session.hpp','rangers_data_session.cpp','rangers_data_session.hpp')]
    sources += [ROOT/'tests/probe_matrixgame_rangers_owned_host.cpp',Path(__file__).resolve(),ROOT/'tools/e2e_dat_cleanup.py',ROOT/'tools/e2e_source_overrides.py']
    before={str(path.relative_to(ROOT)):sha(path) for path in sources}
    env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (BASE/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(label,p.returncode,log[-8000:] if p.returncode else '',flush=True);assert not p.returncode,label
        return log
    try:
        old=ROOT/'port/switch/build/e2e-game-perf515';mapping=read(old/'effective-source-map.json');flags=read(old/'compile-flags.txt')
        objects={name:digest for name,digest in prior['archive_objects_sha256'].items() if not name.endswith(('EC_BlockPar.o','EC_Data.o'))}
        result['dat_cleanup_inputs_sha256']={};result['dat_cleanup_outputs_sha256']={}
        for name in ('EC_BlockPar','EC_Data'):
            original=ROOT/mapping[f'upstream/cpp/src/{name}.cpp']
            source=BASE/'source'/(name+'.cpp');source.parent.mkdir(parents=True,exist_ok=True)
            source.write_text(dat_cleanup(original.read_text(encoding='utf-8'),name+'.cpp'),encoding='utf-8')
            result['dat_cleanup_inputs_sha256'][str(original.relative_to(ROOT))]=sha(original)
            result['dat_cleanup_outputs_sha256'][str(source.relative_to(ROOT))]=sha(source)
            obj=BASE/'dependencies'/(name+'.o');obj.parent.mkdir(parents=True,exist_ok=True)
            run([flags['compiler'],*flags['flags'],'-c',source,'-o',obj],'compile-'+name);objects[str(obj.relative_to(ROOT))]=sha(obj)
        assert len(objects)==378
        archive=BASE/'dependencies/liboriginal-rangers-host-data-cleanup.a';rsp=BASE/'dependencies/objects.rsp'
        rsp.write_text('\n'.join('"'+(ROOT/name).as_posix()+'"' for name in objects)+'\n')
        run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rcs',archive,'@'+str(rsp)],'archive')
        result['dependency_objects_sha256']=objects;result['dependency_archive_sha256']=sha(archive)
        command=next(c['command'] for c in prior['commands'] if c['label']=='configure')[:]
        command[command.index('-B')+1]=str(BASE)
        command += ['-DBRIDGE_HOST_LINK_PROBE_SOURCE='+str(ROOT/'tests/probe_matrixgame_rangers_owned_host.cpp')]
        command += ['-DBRIDGE_RANGERS_HOST_ARCHIVE='+str(archive)]
        run(command,'configure');run([command[0],'--build',BASE,'--target','matrixgame-rangers-host-link-probe','-j','4'],'build')
        elf=BASE/'matrixgame-rangers-host-link-probe.elf'
        symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
        result['strong_undefined']=[line for line in symbols.splitlines() if ' U ' in line];assert not result['strong_undefined']
        required=['RangersDataSession::RangersDataSession(', 'RangersAudioSession::RangersAudioSession(srhd_awa::matrixgame::RangersDataSession&)', 'RangersMusicSession::RangersMusicSession(srhd_awa::matrixgame::RangersDataSession&)']+prior['required_linked_symbols']
        for name in required:assert name in symbols,name
        result['required_linked_symbols']=required
        result['elf_header']=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header')
        assert 'AArch64' in result['elf_header'] and 'DYN (Position-Independent Executable file)' in result['elf_header']
        result['artifacts']={str(path.relative_to(ROOT)):{'sha256':sha(path),'bytes':path.stat().st_size} for path in (elf,BASE/'libmatrixgame-engine-callbacks.a',BASE/'rangers-host-link.map')}
        result['link_inputs_sha256']={str(path.relative_to(ROOT)):sha(path) for path in BASE.rglob('*') if path.is_file() and path.suffix in ('.a','.obj','.o')}
        check_hashes(protected);check_hashes(before)
        result['reused_dependencies_sha256']=prior['archive_objects_sha256'];result['protected_E572_sha256']=protected
        result['status']='PASS; FULL SDK SHARED DATA + BORROWED AUDIO/MUSIC + ENGINE CALLBACKS LINK; NOT RUN'
    finally:
        result['source_sha256']=before;(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

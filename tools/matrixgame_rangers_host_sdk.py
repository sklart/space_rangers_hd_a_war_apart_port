"""Reuse current SDK game objects, rebuild stale inputs privately, link real host.

No original build output or NRO is replaced. Original dependency freshness uses
GCC depfiles and the existing compiler/flags stamp, then pins input SHA256 for
the new archive. Missing prerequisites fail instead of accepting stale objects.
"""
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from e2e_arm64_build import dependency_paths
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e572-rangers-host-sdk'
OLD=ROOT/'port/switch/build/e2e-game-perf515'
CXX=Path('C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe')

def main():
    BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','new_NRO':False}
    if (BASE/'result.json').exists():
        attempt=1
        while (BASE/f'result-attempt-{attempt}.json').exists():attempt+=1
        shutil.copyfile(BASE/'result.json',BASE/f'result-attempt-{attempt}.json')
    env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
        log=p.stdout+p.stderr;logpath=BASE/(label+'.log');attempt=1
        while logpath.exists():logpath=BASE/f'{label}-{attempt}.log';attempt+=1
        logpath.write_text(log,encoding='utf-8')
        result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(logpath.relative_to(ROOT))})
        print(label,p.returncode,log[-9000:] if p.returncode else '',flush=True)
        assert p.returncode==0,label
        return log
    sources=[ROOT/'port/matrixgame/native'/name for name in ('CMakeLists.txt','engine_callbacks.cpp','engine_callbacks.hpp','rangers_host_callbacks.cpp','rangers_host_callbacks.hpp','rangers_audio_session.cpp','rangers_audio_session.hpp','rangers_music_session.cpp','rangers_music_session.hpp','rangers_bridge.cpp','rangers_bridge.hpp')]
    sources += [ROOT/'tests/probe_matrixgame_rangers_host_link.cpp',Path(__file__).resolve()]
    before={str(path.relative_to(ROOT)):sha(path) for path in sources}
    protected=[OLD/'build-report.json',OLD/'compile-flags.txt',OLD/'link-objects.rsp',OLD/'effective-source-map.json',OLD/'okgf-fast24-no-tls/libokgf.a',OLD/'okgf-fast24-no-tls/vendor/softfloat/libokgf_softfloat.a']
    protected += [ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/libmatrixgame-engine-callbacks.a',ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro']
    oldreport=read(OLD/'build-report.json');protected += [ROOT/oldreport['matching_elf']['path'],ROOT/oldreport['nro']['path']]
    result['protected_before']={str(path):sha(path) for path in protected}
    try:
        assert oldreport['upstream_pin']=='57fa689c630193a66fdea6ca4c79a188814991cd'
        flags=read(OLD/'compile-flags.txt');assert run([CXX,'--version'],'compiler').splitlines()[0]==flags['compiler_version']
        objects=[Path(line.strip('"')) for line in (OLD/'link-objects.rsp').read_text().splitlines() if line.strip()]
        assert len(objects)==380 and sum(path.name=='program.o' for path in objects)==1
        objects=[path for path in objects if path.name not in ('program.o','package.o')]
        mapping=read(OLD/'effective-source-map.json');pending=[];reused=[];dependencies=set()
        for obj in objects:
            deps=dependency_paths(obj.with_suffix('.d'));assert deps and all(path.is_file() for path in deps),obj
            dependencies.update(deps)
            stale=[str(path.relative_to(ROOT)) for path in [*deps,OLD/'compile-flags.txt'] if path.stat().st_mtime_ns>obj.stat().st_mtime_ns]
            if not stale:reused.append(obj);continue
            original=(ROOT/'upstream/cpp/runtime/runtime.cpp' if obj.stem=='runtime' else ROOT/'upstream/cpp/src'/(obj.stem+'.cpp'))
            source=ROOT/mapping[str(original.relative_to(ROOT)).replace('\\','/')] if obj.parent.name=='upstream' else ROOT/'port/switch/platform'/(obj.stem+'.cpp')
            assert source in deps,(obj,source)
            dest=BASE/'fresh'/obj.parent.name/obj.name;dest.parent.mkdir(parents=True,exist_ok=True)
            pending.append((source,dest,stale))
        result['dependency_sha256']={str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path):sha(path) for path in sorted(dependencies)}
        result['old_objects_sha256']={str(path.relative_to(ROOT)):sha(path) for path in objects}
        result['excluded_entries']=['upstream/program.o (old original-game main)','platform/package.o (single shared implementation supplied by matrixgame-native-core)']
        result['stale_objects']=[{'source':str(source.relative_to(ROOT)),'output':str(dest.relative_to(ROOT)),'newer_dependencies':stale} for source,dest,stale in pending]
        print('reuse',len(reused),'recompile',len(pending),flush=True)
        def compile_one(item):
            source,dest,_=item
            dep_inputs=dependency_paths(dest.with_suffix('.d')) if dest.with_suffix('.d').exists() else dependency_paths((OLD/dest.parent.name/dest.name).with_suffix('.d'))
            inputs={'dependencies':{str(path):sha(path) for path in dep_inputs},'flags':flags}
            stamp=dest.with_suffix('.build.json')
            if stamp.exists() and dest.exists():
                cached=read(stamp)
                if cached['inputs']==inputs and cached['object_sha256']==sha(dest):
                    result['commands'].append({'label':'reuse-private-'+dest.stem,'exit':0,'command':[]});return dest
            run([CXX,*flags['flags'],'-MMD','-MF',dest.with_suffix('.d'),'-c',source,'-o',dest],'compile-'+dest.parent.name+'-'+dest.stem)
            inputs['dependencies']={str(path):sha(path) for path in dependency_paths(dest.with_suffix('.d'))}
            stamp.write_text(json.dumps({'inputs':inputs,'object_sha256':sha(dest)},indent=2)+'\n')
            return dest
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            fresh=list(pool.map(compile_one,pending))
        for obj in fresh:
            for path in dependency_paths(obj.with_suffix('.d')):
                name=str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
                digest=sha(path)
                assert name not in result['dependency_sha256'] or result['dependency_sha256'][name]==digest,name
                result['dependency_sha256'][name]=digest
        all_objects=reused+fresh;assert len(all_objects)==378
        result['archive_objects_sha256']={str(path.relative_to(ROOT)):sha(path) for path in all_objects}
        # GCC response-file quoting uses forward slash paths, preserving spaces.
        rsp=BASE/'archive-inputs.rsp';rsp.write_text('\n'.join('"'+path.as_posix()+'"' for path in all_objects)+'\n')
        archive=BASE/'liboriginal-rangers-host-shared-package.a'
        run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rcs',archive,'@'+str(rsp)],'archive')
        sdk=BASE/'sdk';command=read(ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/result.json')['commands'][0]['command'][:]
        command[command.index('-B')+1]=str(sdk)
        command+=['-DBRIDGE_BUILD_LINK_PROBE=OFF','-DBRIDGE_BUILD_HOST_LINK_PROBE=ON','-DBRIDGE_RANGERS_HOST_ARCHIVE='+str(archive)]
        command+=['-DBRIDGE_RANGERS_OKGF_ARCHIVE='+str(OLD/'okgf-fast24-no-tls/libokgf.a'),'-DBRIDGE_RANGERS_SOFTFLOAT_ARCHIVE='+str(OLD/'okgf-fast24-no-tls/vendor/softfloat/libokgf_softfloat.a')]
        run(command,'configure');run([command[0],'--build',sdk,'--target','matrixgame-rangers-host-link-probe','-j','4'],'build')
        elf=sdk/'matrixgame-rangers-host-link-probe.elf'
        symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
        undefined=[line for line in symbols.splitlines() if ' U ' in line];assert not undefined,undefined
        producer=(ROOT/'port/matrixgame/native/rangers_host_callbacks.cpp').read_text()
        original_callbacks=re.findall(r'callbacks\.\w+=Robot::TRobot\w+\(Robot::(Robot\w+)\);',producer)
        assert len(original_callbacks)==15
        required=['MakeRangersHostCallbacks(', 'RangersAudioSession::RangersAudioSession(', 'RangersMusicSession::RangersMusicSession(', 'EngineCallbacks::EngineCallbacks(', 'Robot::RobotRenderText(', 'GR_Music::TMusicControl_Create(', 'GR_Sound::TSoundControl_Create(']+['Robot::'+name+'(' for name in original_callbacks]
        for name in required:assert name in symbols,name
        result['required_linked_symbols']=required;result['strong_undefined']=undefined
        result['elf_header']=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header')
        assert 'AArch64' in result['elf_header'] and 'DYN (Position-Independent Executable file)' in result['elf_header']
        check_hashes(result['dependency_sha256']);check_hashes(result['old_objects_sha256']);check_hashes(before)
        result['artifacts']={str(path.relative_to(ROOT)):{'sha256':sha(path),'bytes':path.stat().st_size} for path in (archive,elf,sdk/'rangers-host-link.map')}
        result['native_sdk_link_inputs_sha256']={str(path.relative_to(ROOT)):sha(path) for path in sdk.rglob('*') if path.is_file() and path.suffix in ('.a','.obj','.o')}
        result['status']='PASS; REAL RANGERS HOST AND ENGINE CALLBACKS FULL SDK LINK; NOT RUN'
    finally:
        result['source_sha256']=before
        result['protected_after']={str(path):sha(path) for path in protected};assert result['protected_before']==result['protected_after']
        (BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()

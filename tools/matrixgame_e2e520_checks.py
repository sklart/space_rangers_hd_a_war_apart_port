"""Small staging draw contract on host, ARM64/QEMU, or devkitA64 link only."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    kind=sys.argv[1]
    if kind not in ('host','arm64','sdk'):raise ValueError(kind)
    out=ROOT/'build/matrixgame/e2e520'/('checks-'+kind)
    out.mkdir(parents=True,exist_ok=True)
    compiler={'host':'g++','arm64':'aarch64-linux-gnu-g++','sdk':'C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe'}[kind]
    executable=out/('staging-draw.elf' if kind=='sdk' else 'staging-draw')
    command=[compiler,'-std=c++20','-O1','-g','-UNDEBUG','-I'+str(ROOT/'port/matrixgame/native-core'),
        '-I'+str(ROOT/'port/matrixgame/native-engine'),str(ROOT/'tests/test_matrixgame_staging_draw.cpp'),
        str(ROOT/'port/matrixgame/native-core/buffer_resources.cpp'),'-o',str(executable)]
    if kind=='host':command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    if kind=='sdk':command+=['-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIE',
        '-specs=C:/devkitPro/libnx/switch.specs','-LC:/devkitPro/libnx/lib','-lnx']
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
             DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
    report={'commands':[],'results':{},'runtime_executed':kind!='sdk'}
    def run(cmd,label):
        value=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True)
        report['commands'].append(cmd);report['results'][label]=value.returncode
        (out/(label+'.log')).write_text(value.stdout+value.stderr)
        print(kind,label,value.returncode,value.stdout,value.stderr[-2500:],flush=True)
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        if value.returncode:raise SystemExit(value.returncode)
        return value
    run(command,'compile_link')
    if kind=='sdk':
        value=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-u',str(executable)],'undefined')
        report['strong_undefined']=[x for x in value.stdout.splitlines() if ' U ' in x]
        assert not report['strong_undefined']
        previous=ROOT/'build/matrixgame/e2e519'
        template=json.loads((previous/'skin-borrow-sdk/result.json').read_text())['commands'][0]
        source=ROOT/'build/matrixgame/e2e520/source'
        units={
            'owner':ROOT/'port/matrixgame/native-engine/original_game.cpp',
            'map_device':ROOT/'port/matrixgame/native-engine/map_device_backend.cpp',
            'prepare':source/'MatrixGame/src/OriginalMapPrepare.cpp',
            'map':source/'MatrixGame/src/MatrixMap.cpp',
            'frame':source/'MatrixGame/src/OriginalWindowsFrame.cpp',
        }
        for name,path in units.items():
            command=[x.replace(str(previous/'skin-borrow-source').replace('\\','/'),str(source).replace('\\','/'))
                      .replace(str(previous/'skin-borrow-source'),str(source)) for x in template]
            command[command.index('-c')+1]=str(path)
            command[command.index('-o')+1]=str(out/(name+'.o'))
            run(command,'compile_'+name)
        report['full_game_sdk_link']=False
    else:
        command=[str(executable)]
        if kind=='arm64':command=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']+command
        run(command,'runtime')
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()

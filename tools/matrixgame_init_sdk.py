"""Link cache/CPU-entry with real deko upload or owned draw session; no NRO/run."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sdk',type=Path,default=Path('C:/devkitPro'))
    p.add_argument('--resume',action='store_true',help='Incremental rebuild of this tool\'s existing managed SDK output')
    p.add_argument('--cached-frame',action='store_true',help='Link the actual owned deko session/cache/VO/draw path instead of the earlier upload-only probe')
    p.add_argument('--skin-resources',action='store_true',help='Link original skin resource selection/Takt and automatic VO surface texture draw')
    p.add_argument('--skin-callbacks',action='store_true',help='Link real SSkin callbacks and original VO Draw with native deko submission')
    p.add_argument('--animation',action='store_true',help='Link original no-light animation Takt -> actual animated deko Draw')
    p.add_argument('--bootstrap',action='store_true',help='Compile/link the shared cache/VO/platform bootstrap driver; explicit unsupported window on Switch')
    a=p.parse_args();out=a.output.resolve();sdk=a.sdk.resolve()
    if not out.is_relative_to(ROOT/'build'):raise ValueError('Use a build/ output')
    if out.exists():
        if not a.resume or not (out/'result.json').is_file() or not (out/'cmake/CMakeCache.txt').is_file():
            raise ValueError('Use a fresh output or --resume for a managed SDK result')
        index=1
        while (out/f'result-before-resume-{index}.json').exists():index+=1
        backup=out/f'result-before-resume-{index}.json'
        shutil.copy2(out/'result.json',backup)
        for log in ('build.log','configure.log'):
            if (out/log).exists():shutil.copy2(out/log,out/(f'before-resume-{index}-'+log))
    else:out.mkdir(parents=True)
    suffix='.exe' if os.name=='nt' else ''
    vs=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake')
    cmake=vs/'CMake/bin/cmake.exe' if os.name=='nt' else Path(shutil.which('cmake'))
    args=[]
    if os.name=='nt':args=['-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(vs/'Ninja/ninja.exe')]
    env={**os.environ,'DEVKITPRO':sdk.as_posix(),'DEVKITA64':(sdk/'devkitA64').as_posix()}
    report=dict(status='RUNNING',hardware_pending=True,runtime_executed=False,nro_packaged=False,commands=[])
    def run(command,log):
        command=list(map(str,command));report['commands'].append(command)
        result=subprocess.run(command,cwd=ROOT,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (out/log).write_text(result.stdout,encoding='utf-8')
        if result.returncode:raise RuntimeError(log+': exit '+str(result.returncode))
        return result.stdout
    try:
        run([cmake,'-S',ROOT/'port/matrixgame/native-engine','-B',out/'cmake',*args,
            '-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'port/matrixgame/native-core/switch-toolchain.cmake'),
            '-DMATRIXGAME_DEVKITPRO='+sdk.as_posix(),'-DBUILD_TESTING=OFF','-DCMAKE_BUILD_TYPE=Release',
            '-DPython3_EXECUTABLE='+sys.executable,
            '-DMATRIXGAME_INIT_BUILD_LINK_PROBE=ON','-DMATRIXGAME_SHADER_DIR='+str(ROOT/'build/matrixgame/deko'),
            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'],'configure.log')
        target='matrixgame-native-animation-probe' if a.animation else ('matrixgame-native-callbacks-probe' if a.skin_callbacks else ('matrixgame-native-skin-probe' if a.skin_resources else ('matrixgame-native-frame-probe' if a.cached_frame else 'matrixgame-native-init-probe')))
        if a.bootstrap:target='matrixgame-native-bootstrap'
        run([cmake,'--build',out/'cmake','--target',target,'--parallel','4'],'build.log')
        elf=out/('cmake/'+target+'.elf')
        readelf=sdk/('devkitA64/bin/aarch64-none-elf-readelf'+suffix)
        header=run([readelf,'-h',elf],'elf-header.log');assert 'AArch64' in header and 'ELF64' in header
        symbols=run([readelf,'-sW',elf],'symbols.log')
        undefined=[line for line in symbols.splitlines() if ' UND ' in line and ' WEAK ' not in line and len(line.split())>7]
        assert not undefined,undefined
        required=('MatrixGameNativeAnimationDraw','NativeVectorObjectAnim','DrawAnimated','PrepareAnimated') if a.animation else (('MatrixGameNativeCallbackDraw','DrawSkinned','BeforeDraw','LoadSpecial') if a.skin_callbacks else (('MatrixGameNativeSkinDraw','DrawSurfaceTextures','SurfaceMaterials','Takt','Acquire') if a.skin_resources else (('MatrixGameNativeSessionDraw','NativeDekoSession','PrepareNativeFrame','DrawFrame') if a.cached_frame else ('MatrixGameCachedMaterialGpuUpload',))))
        if a.bootstrap:required=('AttachDevice','WindowProcedure','InitAsDLL','CacheInit','L3GInitAsDLLCpuPhase','CMatrixSampleStateManager','S3D_Default','CDeviceState','StoreState','RestoreState','InitializeGameDefaults','InitializeOriginalGameDefaults','ClearOriginalGameDefaults')
        else:required=(*required,'dkDeviceCreate','SetTextureAsset','CacheInit','L3GInitAsDLLCpuPhase')
        for name in required:
            assert name in symbols,name
        notes=run([readelf,'-n',elf],'notes.log')
        report.update(status='SDK_LINK_PASS',undefined_strong=undefined,elf=str(elf),
            sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),bytes=elf.stat().st_size,
            build_id=next(line.strip() for line in notes.splitlines() if 'Build ID:' in line))
    except Exception as error:report.update(status='FAIL',error=str(error))
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='commands'},indent=2))
    return int(report['status']!='SDK_LINK_PASS')


if __name__=='__main__':raise SystemExit(main())

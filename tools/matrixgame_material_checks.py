"""Address checks for original texture cascades; never creates an NRO."""
import argparse,hashlib,json,os,struct,subprocess,sys
from pathlib import Path
from matrixgame_material_callbacks_prepare import prepare
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/matrixgame/e2e521/materials'

def main():
    args=argparse.ArgumentParser();args.add_argument('kind',choices=('host','arm64','windows','sdk','cmake-windows','cmake-sdk'))
    args.add_argument('--output',type=Path,default=OUT);args.add_argument('--diagnose',action='store_true');options=args.parse_args();kind=options.kind
    out=options.output.resolve()/kind;out.mkdir(parents=True,exist_ok=True)
    evidence={'scope':'original material/vertex host checks; native provider SDK-only; no native Init/frame/hardware PASS',
              'commands':[],'status':'INCOMPLETE','Switch_runtime':False,'NRO_created':False,
              'extraction':prepare(out/'callbacks')}
    def run(command,label,env=None):
        p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',env=env or dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'))
        evidence['commands'].append({'label':label,'command':list(map(str,command)),'exit':p.returncode})
        (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        print(kind,label,p.returncode,(p.stdout+p.stderr)[-6000:] if p.returncode or label=='runtime' else '',flush=True)
        if p.returncode:raise RuntimeError(label+' failed')
        return p.stdout
    try:
        sources=[ROOT/'tests/test_matrixgame_materials.cpp',ROOT/'port/matrixgame/deko/material_combiner.cpp',ROOT/'port/matrixgame/deko/fixed_vertex.cpp',ROOT/'port/matrixgame/native-engine/platform_3g.cpp']
        includes=[out/'callbacks',ROOT/'port/matrixgame/deko',ROOT/'port/matrixgame/native-engine',ROOT/'port/matrixgame/native-core']
        if kind.startswith('cmake-'):
            source=out/'source';source.mkdir(exist_ok=True)
            (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(MatrixGameMaterialIntegration LANGUAGES CXX)\ninclude(CTest)\nfind_package(Python3 REQUIRED COMPONENTS Interpreter)\nset(PORT_ROOT "'+ROOT.as_posix()+'")\ninclude("'+(ROOT/'port/matrixgame/deko/materials.cmake').as_posix()+'")\n')
            bundled=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake')
            cmake=bundled/'CMake/bin/cmake.exe';build=out/'build'
            options=['-G','Visual Studio 17 2022','-A','Win32','-DBUILD_TESTING=ON']
            target='matrixgame-material-contract'
            if kind=='cmake-sdk':
                options=['-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(bundled/'Ninja/ninja.exe'),
                    '-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'port/matrixgame/native-core/switch-toolchain.cmake'),
                    '-DMATRIXGAME_DEVKITPRO=C:/devkitPro','-DBUILD_TESTING=OFF']
                target='matrixgame-material-combiner'
            run([cmake,'-S',source,'-B',build,*options,'-DPython3_EXECUTABLE='+sys.executable],'configure')
            run([cmake,'--build',build,'--config','Release','--target',target,'--parallel','2'],'build')
            evidence['runtime_executed']=False
        elif kind in ('host','arm64'):
            def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
            prefix=['wsl','--exec'] if os.name=='nt' else []
            cxx='g++' if kind=='host' else 'aarch64-linux-gnu-g++'
            target=out/'material-checks'
            cmd=[*prefix,cxx,'-std=c++20','-O1','-g','-UNDEBUG','-ffp-contract=off',*[f'-I{linux(p)}' for p in includes],*map(linux,sources),'-o',linux(target)]
            if kind=='host':cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
            run(cmd,'build')
            cmd=[*prefix,'env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            evidence['stdout']=run([*cmd,linux(target)],'runtime')
            routing_sources=[ROOT/'tests/test_matrixgame_native_graphics.cpp',ROOT/'port/matrixgame/native-engine/map_device_backend.cpp',ROOT/'port/matrixgame/native-engine/platform_3g.cpp']
            route_target=out/'native-graphics-checks'
            route_includes=[*includes,ROOT/'build/matrixgame/e2e521/source/MatrixLib/3G',ROOT/'build/matrixgame/e2e521/source/portable-core']
            route_cmd=[*prefix,cxx,'-std=c++20','-O1','-g','-UNDEBUG',*[f'-I{linux(p)}' for p in route_includes],*map(linux,routing_sources),'-o',linux(route_target)]
            if kind=='host':route_cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
            run(route_cmd,'routing-build');evidence['routing_stdout']=run([*cmd,linux(route_target)],'routing-runtime')
        elif kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            command=f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /O2 /UNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN '
            command+=' '.join('/I"'+str(p)+'"' for p in includes)+' '+' '.join('"'+str(p)+'"' for p in sources)
            command+=' /Fo"'+out.as_posix()+'/" /Fe"'+str(out/'material-checks.exe')+'" /link d3d9.lib user32.lib gdi32.lib opengl32.lib'
            script=out/'build.cmd';script.write_text(command+'\n')
            run(['cmd.exe','/d','/c',script],'build')
            evidence['stdout']=run([out/'material-checks.exe',*(['--diagnose'] if options.diagnose else ['--fragment',ROOT/'port/matrixgame/deko/material.frag.glsl'])],'runtime')
        else:
            sdk=Path('C:/devkitPro');cxx=sdk/'devkitA64/bin/aarch64-none-elf-g++.exe'
            shader=out/'material.frag.dksh'
            run([sys.executable,ROOT/'tools/matrixgame_material_shader_build.py','--output',out,'--sdk',sdk],'uam')
            data=shader.read_bytes();magic,head,control,code,programs,count=struct.unpack_from('<6I',data)
            assert magic==0x48534b44 and 24<=head<=programs<control and control+code==len(data) and code and count==1
            evidence['dksh']={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
            flags=['-std=c++20','-O2','-UNDEBUG','-ffp-contract=off','-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIE','-D__SWITCH__',*[f'-I{p}' for p in includes],'-I'+str(out),'-IC:/devkitPro/libnx/include']
            sources+=[ROOT/'port/matrixgame/deko/material_program.cpp',ROOT/'port/matrixgame/native-engine/platform_switch.cpp',ROOT/'port/matrixgame/native-core/buffer_resources.cpp']
            objects=[]
            for p in sources:
                obj=out/(p.stem+'.o');run([cxx,*flags,'-c',p,'-o',obj],p.stem);objects.append(obj)
            target=out/'material-checks.elf'
            run([cxx,'-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIE','-specs=C:/devkitPro/libnx/switch.specs','-Wl,--no-gc-sections',*objects,'-LC:/devkitPro/libnx/lib','-ldeko3d','-lnx','-o',target],'link')
            symbols=run([sdk/'devkitA64/bin/aarch64-none-elf-nm.exe','-C',target],'symbols')
            for name in ('DekoCombinerProgram::Prepare','DekoCombinerProgram::Bind','dkShaderInitialize','dkCmdBufBindTextures','dkCmdBufBindUniformBuffer'):
                assert name in symbols,name
            strong=[v for v in symbols.splitlines() if ' U ' in v];assert not strong,strong
            evidence['strong_undefined']=strong
        inputs=sources+[ROOT/'port/matrixgame/deko/material_combiner.hpp',ROOT/'port/matrixgame/deko/material_program.hpp',ROOT/'port/matrixgame/deko/fixed_vertex.hpp',ROOT/'port/matrixgame/deko/material.frag.glsl',ROOT/'port/matrixgame/deko/material.vert.glsl',ROOT/'port/matrixgame/deko/materials.cmake',ROOT/'tools/matrixgame_material_shader_build.py',ROOT/'tests/matrixgame_material_gl_reference.hpp',ROOT/'tests/matrixgame_fixed_vertex_reference.hpp',ROOT/'tools/matrixgame_material_callbacks_prepare.py',ROOT/'tests/test_matrixgame_native_graphics.cpp',ROOT/'port/matrixgame/native-engine/map_device_backend.cpp',ROOT/'port/matrixgame/native-engine/native_graphics.hpp',ROOT/'port/matrixgame/native-engine/platform_switch.hpp',ROOT/'build/matrixgame/e2e521/source/MatrixLib/3G/device_state_registers.hpp',Path(__file__).resolve()]
        evidence['input_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
        evidence['status']='PASS'
    except Exception as e:
        evidence.update(status='FAIL',error=str(e));raise
    finally:
        (out/'result.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()

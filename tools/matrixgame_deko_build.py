"""Compile/link the first deko3d mesh pass; never package an extra NRO or claim hardware proof."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

def main() -> int:
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/matrixgame/deko'
    out.mkdir(parents=True, exist_ok=True)
    report_path = out / 'build-report.json'
    report = {'scope': 'DEKO MESH PASS COMPILE/LINK ONLY; NO GPU EXECUTION',
              'status': 'INCOMPLETE', 'hardware': 'NOT TESTED', 'nro_packaged': False}
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    sdk = Path(os.environ.get('DEVKITPRO', 'C:/devkitPro' if os.name == 'nt' else '/opt/devkitpro'))
    if os.name == 'nt' and not sdk.is_dir():
        sdk = Path('C:/devkitPro')
    suffix = '.exe' if os.name == 'nt' else ''
    cxx = sdk / ('devkitA64/bin/aarch64-none-elf-g++' + suffix)
    uam = sdk / ('tools/bin/uam' + suffix)
    readelf = sdk / ('devkitA64/bin/aarch64-none-elf-readelf' + suffix)
    source_dir = root / 'port/matrixgame/deko'
    inputs = [p for p in source_dir.iterdir() if p.is_file()]
    inputs += [p for p in (root/'port/matrixgame/native-core').iterdir() if p.is_file()]
    inputs += list((root/'tools').glob('matrixgame_*overrides.py')) + [root/'tools/matrixgame_core_prepare.py']
    inputs += [root/'tools/matrixgame_bc_prepare.py']
    inputs += [p for p in (root/'port/matrixgame/third_party').rglob('*') if p.is_file()]
    inputs += [root / 'tests/probe_matrixgame_deko_link.cpp', Path(__file__).resolve()]
    inputs += [root / 'tests/matrixgame_raster_gpu_probe.hpp', root / 'tests/matrixgame_sampler_gpu_probe.hpp']
    log = []
    def run(command):
        result = subprocess.run(list(map(str, command)), cwd=root, capture_output=True, text=True,
                                env={**os.environ, 'DEVKITPRO': sdk.as_posix(), 'DEVKITA64': (sdk / 'devkitA64').as_posix()})
        log.append(result.stdout + result.stderr)
        (out / 'build.log').write_text('\n'.join(log), encoding='utf-8')
        if result.returncode:
            raise RuntimeError(f'{Path(command[0]).name} exit {result.returncode}: {result.stderr[-2500:]}')
        return result.stdout.strip()
    def require(condition, message):
        if not condition:
            raise ValueError(message)
    try:
        report['compiler'] = run([cxx, '--version']).splitlines()[0]
        report['uam'] = run([uam, '--version'])
        report['deko3d_header_sha256'] = hashlib.sha256((sdk / 'libnx/include/deko3d.h').read_bytes()).hexdigest()
        report['deko3d_library_sha256'] = hashlib.sha256((sdk / 'libnx/lib/libdeko3d.a').read_bytes()).hexdigest()
        report['jpeg_header_sha256'] = hashlib.sha256((sdk/'portlibs/switch/include/jpeglib.h').read_bytes()).hexdigest()
        report['jpeg_library_sha256'] = hashlib.sha256((sdk/'portlibs/switch/lib/libjpeg.a').read_bytes()).hexdigest()
        shaders = []
        for stage, filename in [('vert', 'mesh.vert.glsl'), ('frag', 'mesh.frag.glsl')]:
            target = out / (stage + '.dksh')
            run([uam, '-s', stage, '-o', target, source_dir / filename])
            data = target.read_bytes()
            require(len(data) >= 24, 'short compiled DKSH')
            magic, head, control, code, programs, count = struct.unpack_from('<6I', data)
            require(magic == 0x48534b44 and 24 <= head <= control and control + code == len(data), 'DKSH header/size mismatch')
            require(code and head <= programs < control and count == 1, 'DKSH program extent mismatch')
            shaders.append(data)
        header = '#pragma once\n#include <array>\n#include <cstdint>\n'
        for name, data in zip(['vertex_dksh', 'fragment_dksh'], shaders):
            header += 'inline constexpr std::array<std::uint8_t,' + str(len(data)) + '> ' + name + '{' + ','.join(map(str, data)) + '};\n'
        (out / 'mesh_shaders.hpp').write_text(header)
        cmake = shutil.which('cmake')
        configure = []
        if os.name == 'nt':
            bundled = Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake')
            if (bundled/'CMake/bin/cmake.exe').is_file(): cmake = str(bundled/'CMake/bin/cmake.exe')
            ninja = bundled/'Ninja/ninja.exe'
            if ninja.is_file(): configure = ['-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(ninja)]
        require(cmake, 'CMake not found')
        build = out/'cmake'
        run([cmake,'-S',source_dir,'-B',build,*configure,
             '-DCMAKE_TOOLCHAIN_FILE='+str(root/'port/matrixgame/native-core/switch-toolchain.cmake'),
             '-DMATRIXGAME_DEVKITPRO='+sdk.as_posix(),'-DMATRIXGAME_SHADER_DIR='+str(out),
             '-DBUILD_TESTING=OFF','-DCMAKE_BUILD_TYPE=Release'])
        run([cmake,'--build',build,'--target','matrixgame-deko-mesh-probe','--parallel','2'])
        elf = out / 'matrixgame-deko-mesh-probe.elf'
        shutil.copyfile(build/elf.name,elf)
        report['native_core_provenance'] = json.loads((build/'core/source/source-provenance.json').read_text())
        machine = run([readelf, '-h', elf])
        require('AArch64' in machine and 'ELF64' in machine, 'non-ARM64 ELF')
        symbols = run([readelf, '-sW', elf])
        required = ['dkDeviceCreate', 'dkQueueCreate', 'dkCmdBufDrawIndexed', 'dkCmdBufCopyImageToBuffer', 'dkCmdBufCopyBufferToImage', 'dkCmdBufBindTextures', 'VectorGeometry4Load', 'VectorGeometry11FrameRanges', 'UploadPngTexture', 'ReadStart_RGBA']
        required += ['SetTextureAsset', 'TextureAsset6Decode']
        required += ['TextureRequest7Resolve', 'ParseOriginalTextureFlags']
        required += ['dkCmdBufBindBlendStates', 'dkCmdBufSetAlphaRef']
        required += ['DecodeJpegTexture','jpeg_read_header','jpeg_start_decompress','jpeg_read_scanlines','jpeg_destroy_decompress']
        required += ['PrepareMips','D3DXEncodeBC1','D3DXEncodeBC2','D3DXEncodeBC3']
        required += ['DekoBufferRenderer4Draw','CaptureBoundVO','ValidateBoundVO','MeshRevision']
        require(all(name in symbols for name in required), 'GPU draw/readback implementation missing from ELF')
        undefined = [line for line in symbols.splitlines() if ' UND ' in line and ' WEAK ' not in line and len(line.split()) > 7]
        require(not undefined, 'unresolved strong symbols: ' + str(undefined))
        artifacts = [elf, *[out / (stage + '.dksh') for stage in ['vert', 'frag']]]
        report.update(status='PASS', required_deko_symbols=required,
                      source_sha256={p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
                      artifacts={p.relative_to(root).as_posix(): {'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in artifacts})
    except Exception as error:
        report.update(status='FAIL', error=str(error))
    finally:
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['status'] == 'PASS' else 1

if __name__ == '__main__':
    sys.exit(main())

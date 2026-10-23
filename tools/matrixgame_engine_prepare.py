"""Prepare a disposable full-engine source view; never provide fake backend functions."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from matrixgame_core_prepare import prepare, SOURCE_SUBDIRS


def prepare_engine(output):
    root=Path(__file__).resolve().parents[1]
    original=root/'upstream/matrixgame'
    output=output.resolve()
    if output.is_relative_to((root/'upstream').resolve()):raise ValueError('Refusing upstream output')
    marker=output/'native-engine-source.json'
    if output.exists() and any(output.iterdir()) and not marker.is_file():raise ValueError('Existing unmanaged source output')
    git='C:/Program Files/Git/cmd/git.exe' if Path('C:/Program Files/Git/cmd/git.exe').is_file() else 'git'
    git_root=str(original)
    wsl_git=Path('/mnt/c/Program Files/Git/cmd/git.exe')
    if git=='git' and wsl_git.is_file():
        git=str(wsl_git)
        git_root=subprocess.check_output(['wslpath','-w',str(original)],text=True).strip()
    revision=subprocess.check_output([git,'-C',git_root,'rev-parse','HEAD'],text=True).strip()
    if revision!='218115a72fa9c5d43269f4ca84a8596a0bcb82f1':raise ValueError('Engine source pin changed')
    if subprocess.check_output([git,'-C',git_root,'status','--porcelain'],text=True).strip():raise ValueError('Engine submodule is dirty')
    output.mkdir(parents=True,exist_ok=True)
    marker.write_text(json.dumps({'schema':1,'source_commit':revision,'state':'preparing'})+'\n')
    report=prepare(output/'portable-core')
    paths={}
    for tree in ('MatrixLib','MatrixGame'):
        for source in (original/tree).rglob('*'):
            if source.is_file() and source.suffix.lower() in ('.c','.cpp','.h','.hpp','.inl'):
                relative=source.relative_to(original)
                data=source.read_bytes().replace(b'\r\n',b'\n')
                if source.suffix.lower() in ('.cpp','.h','.hpp'):
                    data=re.sub(rb'\brandom::',b'matrixgame_random::',data)
                target=output/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
                paths[relative.as_posix()]=hashlib.sha256(data).hexdigest()
    for name in report['inputs_sha256']:
        relative=(original/'MatrixLib/Base'/SOURCE_SUBDIRS.get(name,'')/name).resolve().relative_to(original.resolve())
        data=(output/'portable-core'/name).read_bytes()
        (output/relative).write_bytes(data);paths[relative.as_posix()]=hashlib.sha256(data).hexdigest()
    # The standalone Windows entry is not the native launcher. Game methods remain intact.
    from matrixgame_core_prepare import replace_body
    main=output/'MatrixGame/src/MatrixGame.cpp'
    text=main.read_text(encoding='utf-8');text=replace_body(text,'int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)','')
    main.write_bytes(text.encode());paths['MatrixGame/src/MatrixGame.cpp']=hashlib.sha256(main.read_bytes()).hexdigest()
    from matrixgame_navigation_overrides import patch_navigation
    paths.update(patch_navigation(output))
    # The complete release cache TU now uses explicit portable resource
    # factories. The declaration overlay below shares the VO ABI; concrete
    # graphics implementations keep their explicit SDK dependencies.
    cache_data=(output/'portable-core/CacheNative.cpp').read_bytes()
    (output/'MatrixLib/3G/Cache.cpp').write_bytes(cache_data)
    paths['MatrixLib/3G/Cache.cpp']=hashlib.sha256(cache_data).hexdigest()
    cpu_header=(output/'portable-core/VectorObjectCpu.hpp').read_bytes()
    (output/'MatrixLib/3G/VectorObjectCpu.hpp').write_bytes(cpu_header)
    paths['MatrixLib/3G/VectorObjectCpu.hpp']=hashlib.sha256(cpu_header).hexdigest()
    shared_header=(output/'portable-core/VectorObjectShared.hpp').read_bytes()
    (output/'MatrixLib/3G/VectorObjectShared.hpp').write_bytes(shared_header)
    paths['MatrixLib/3G/VectorObjectShared.hpp']=hashlib.sha256(shared_header).hexdigest()
    from matrixgame_engine_header_overrides import patch_engine_headers
    paths.update(patch_engine_headers(output))
    from matrixgame_3g_header_overrides import patch_3g_headers
    paths.update(patch_3g_headers(output))
    # Full DeviceState TU and the same shared ordinal types are visible to the
    # opt-in engine/matrixlib targets as well as the integrated native driver.
    registry='MatrixLib/3G/device_state_registers.hpp'
    data=(output/'portable-core/device_state_registers.hpp').read_bytes()
    (output/registry).write_bytes(data);paths[registry]=hashlib.sha256(data).hexdigest()
    # Normalize after pinned algorithm overlays validate their original input.
    # These are fixed-width data fields, not Windows/device API emulation.
    from matrixgame_resource_header_overrides import patch_resource_header
    for name in paths:
        path=output/name;data=path.read_bytes();before_scalars=data
        for old,new in [(b'DWORD',b'std::uint32_t'),(b'WORD',b'std::uint16_t'),(b'BYTE',b'std::uint8_t')]:
            data=re.sub(rb'\b'+old+rb'\b',new,data)
        if data!=before_scalars:
            data=b'#include <cstdint>\n'+data;path.write_bytes(data)
            paths[name]=hashlib.sha256(data).hexdigest()
        adapted=patch_resource_header(name,data)
        if adapted!=data:
            path.write_bytes(adapted);paths[name]=hashlib.sha256(adapted).hexdigest()
    from matrixgame_config_overrides import prepare_config
    paths.update(prepare_config(output))
    from matrixgame_game_defaults_overrides import prepare_game_defaults
    paths.update(prepare_game_defaults(output))
    from matrixgame_cursor_overrides import prepare_cursor
    paths.update(prepare_cursor(output))
    from matrixgame_interface_header_overrides import prepare_interface_headers
    paths.update(prepare_interface_headers(output))
    from matrixgame_video_overrides import prepare_video
    paths.update(prepare_video(output))
    from matrixgame_static_resources_overrides import prepare_static_resources
    paths.update(prepare_static_resources(output))
    from matrixgame_load_progress_overrides import prepare_load_progress
    paths.update(prepare_load_progress(output))
    from matrixgame_render_pipeline_overrides import prepare_render_pipeline
    paths.update(prepare_render_pipeline(output))
    from matrixgame_legacy_texture_overrides import prepare_legacy_texture
    paths.update(prepare_legacy_texture(output))
    from matrixgame_camera_overrides import prepare_camera
    paths.update(prepare_camera(output))
    from matrixgame_map_logic_overrides import prepare_map_logic
    paths.update(prepare_map_logic(output))
    from matrixgame_input_overrides import prepare_input
    paths.update(prepare_input(output))
    from matrixgame_map_device_overrides import prepare_map_device
    paths.update(prepare_map_device(output))
    from matrixgame_console_overrides import prepare_console
    paths.update(prepare_console(output))
    from matrixgame_debug_font_overrides import prepare_debug_font
    paths.update(prepare_debug_font(output))
    from matrixgame_terrain_overrides import prepare_terrain
    paths.update(prepare_terrain(output))
    from matrixgame_bottom_texture_overrides import prepare_bottom_texture
    paths.update(prepare_bottom_texture(output))
    from matrixgame_water_overrides import prepare_water
    paths.update(prepare_water(output))
    result={'schema':1,'source_commit':revision,'scope':'full engine compilation input; runtime/backend not implemented here',
        'files_sha256':paths,'portable_inputs':report['inputs_sha256'],
        'excluded_platform_launcher':'MatrixGameDll.cpp; native entry remains to implement'}
    marker.write_text(json.dumps(result,indent=2)+'\n')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    report=prepare_engine(a.output);print(json.dumps({'source_commit':report['source_commit'],'files':len(report['files_sha256'])}))

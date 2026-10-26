"""Audit the E2E-5.21 material continuation without promoting a subset to caps."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/materials'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8'))

def main():
    results={}
    for kind in ('host','arm64','windows','sdk','cmake-windows','cmake-sdk'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for name,expected in j['input_sha256'].items():assert digest(ROOT/name)==expected,(kind,name)
        if kind in ('host','arm64','windows'):
            match=re.search(r'accepted=(\d+) disabled_alpha=(\d+) null_texture=(\d+) D3D9_differential=(\d+) mismatches=(\d+) diagnostic_cases=(\d+)',j['stdout'])
            assert match,(kind,j['stdout'])
            accepted,alpha,null,gpu,mismatch,diagnostics=map(int,match.groups())
            assert accepted==160 and alpha==163 and null==46 and mismatch==0
            assert gpu==(160 if kind=='windows' else 0)
            assert diagnostics==(209 if kind=='windows' else 0)
            log=(BASE/kind/'runtime.log').read_text(encoding='utf-8')
            assert 'AddressSanitizer' not in log and 'runtime error:' not in log
        results[kind]=j
    old=read(ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json')
    for name in ('port/matrixgame/native-engine/original_game.cpp','tools/matrixgame_original_game_overrides.py',
                 'tools/matrixgame_render_pipeline_overrides.py','tools/matrixgame_skin_manager_overrides.py'):
        assert digest(ROOT/name)==old['source_and_artifact_sha256_at_audit'][name],name
    protected=old['protected']
    for name,entry in protected.items():
        if entry.get('status')=='UNAVAILABLE':continue
        assert digest(Path(name))==entry['sha256'],name
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for name,entry in hardware.items():
        p=ROOT/'logs/2026.10.23'/name
        assert p.stat().st_size==entry['bytes'] and digest(p)==entry['sha256'],name
    prior=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
    assert digest(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')==prior['package_sha256']
    for name,h in prior['source_cfg_sha256'].items():assert digest(ROOT/'upstream/matrixgame/MatrixGame/CFG'/name)==h
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json')
    assert digest(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    artifacts={}
    for path in BASE.rglob('*'):
        if path.is_file() and path.suffix in ('.dksh','.elf','.exe','.a','.lib','.o','.obj','.log','.json','.inc'):
            artifacts[path.relative_to(ROOT).as_posix()]={'sha256':digest(path),'bytes':path.stat().st_size}
    for name in ('tools/matrixgame_material_audit.py','reports/matrixgame-e2e521-materials.md',
                 'port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/CMakeLists.txt',
                 'port/matrixgame/deko/CMakeLists.txt'):
        p=ROOT/name;artifacts[name]={'sha256':digest(p),'bytes':p.stat().st_size}
    sdk_files={}
    for name in ('libnx/include/deko3d.h','libnx/include/deko3d.hpp','libnx/lib/libdeko3d.a','libnx/lib/libnx.a',
                 'tools/bin/uam.exe','devkitA64/bin/aarch64-none-elf-g++.exe'):
        sdk_files[name]=digest(Path('C:/devkitPro')/name)
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    assert parent==old['parent'],'unexpected HEAD change'
    evidence={'stage':'E2E-5.21 PARTIAL; full original material contract NOT CLOSED',
        'date':'2026-10-25','parent':parent,'results':results,
        'D3D9_reference_scope':'prelit vertices, constant 1x1 textures, raw combiner output; raster blend, alpha test, transforms and lighting excluded',
        'D3D9_comparisons':160,'original_callback_comparisons':115,'extra_equation_modifier_comparisons':45,
        'native_material_cases_deliberately_refused':209,
        'disabled_alpha_refusals':163,'null_texture_refusals':46,
        'unsupported_case_D3D9_readbacks':209,'unsupported_case_native_parity':False,
        'ReadCaps_returns_success':False,'module_activated_as_full_DekoDevice_renderer':False,
        'current_native_chain_boundary':'CGame::Init -> EnterPlatform -> EnterNativePlatform -> InitAsNative -> DekoDevice::ReadCaps',
        'known_pending_original_methods':['obj_side2','obj_side6','TerSurf','TerBotM'],
        'remaining_for_map':['disabled-alpha and null-texture observable parity','FVF vertex lighting and TCI/texture transforms',
            'complete raster/stencil/shadow state and original draw submission','texture residency/allocation plus original texture backend',
            'state capture, video/lifecycle, gamma/color, font/cursor/input','native Init map load and acquire/submit/present/readback'],
        'native_Init_executed_this_continuation':False,'Switch_hardware_runtime':False,'NRO_created':False,
        'full_Windows_Init_frame_repeated':False,'original_Init_Draw_algorithms_modified':False,
        'protected':protected,'hardware_files_verified':len(hardware),'original_package_CFG_and_frozen_cache_unchanged':True,
        'source_and_artifact_sha256':artifacts,'sdk_sha256':sdk_files,
        'main_game_P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN',
        'manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN','commit':False,'push':False}
    target=ROOT/'reports/matrixgame-e2e521-materials-evidence.json'
    target.write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':evidence['stage'],'D3D9_comparisons':160,'pending_cases':209,
        'protected_verified':sum(x.get('unchanged',False) for x in protected.values()),'hardware_files_verified':len(hardware)},ensure_ascii=False))

if __name__=='__main__':main()

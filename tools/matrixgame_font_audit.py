"""Audit exact original debug-font software evidence; never executes Switch or produces NRO."""
import json,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/font'
def main():
    results={}
    for kind in ('windows','host','arm64','sdk'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        for n,h in j['input_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        if kind=='sdk':
            assert not j['strong_undefined'] and sha(BASE/kind/'native-entry.elf')==j['elf_sha256']
            for n,h in j['link_inputs_sha256'].items():assert sha(Path(n))==h,n
        elif kind=='windows':
            assert 'cases=699 alpha_cases=4 after_reset_pixels=matched' in j['stdout']
        else:
            assert 'original_HAL_goldens=699 original_CMatrixDebugInfo_lifecycle=executed' in j['stdout']
            assert 'GPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN' in j['stdout']
            assert 'AddressSanitizer' not in j['stdout'] and 'runtime error:' not in j['stdout']
        results[kind]=j
    asset=(BASE/'windows/original-font.bin').read_bytes()
    for binary in ('sdk/original-font.o','sdk/native-entry.elf','cmake-sdk/debug-font/original-font.o'):
        assert asset in (BASE/binary).read_bytes(),binary
    boundary=read(BASE/'boundary/result.json');assert boundary['status']=='CONFIRMED_UNSUPPORTED' and boundary['texture_uploads']==0
    for key in ('source_sha256','input_sha256'):
        for n,h in boundary[key].items():assert sha(ROOT/n)==h,n
    inventory=read(BASE/'map-inventory.json')
    tables={r['name']:{c['name']:c for c in r['columns']} for r in inventory['records']}
    assert len(tables['texunions']['Data']['lengths'])==6 and set(tables['texunions']['Data']['lengths'])=={1024}
    assert len(tables['bottom']['Data']['lengths'])==1534 and len(tables['bitmaps']['Bitmap']['lengths'])==23
    assert sha(BASE/'map-first-source.png')==inventory['first_source_png']['sha256']
    assert 'Build files have been written' in (BASE/'cmake-configure.log').read_text(encoding='utf-8')
    assert 'original-font.o' in (BASE/'cmake-font-object.log').read_text(encoding='utf-8')
    previous=read(ROOT/'reports/matrixgame-e2e521-cursor-evidence.json')
    protected=previous['protected'];checked=0
    for n,v in protected.items():
        if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
    for n,v in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
    primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
    package=ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg'
    assert sha(package)=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
    init=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
    for n,h in init['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    original=read(ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json')
    for n in ('port/matrixgame/native-engine/original_game.cpp','tools/matrixgame_original_game_overrides.py',
        'tools/matrixgame_skin_manager_overrides.py','tools/matrixgame_render_pipeline_overrides.py'):
        assert sha(ROOT/n)==original['source_and_artifact_sha256_at_audit'][n],n

    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
    names=['port/matrixgame/native-engine/'+n for n in ('native_debug_font.hpp','native_debug_font.cpp','native_debug_font_asset.cpp','debug_font_backend.cpp','platform_switch.cpp','platform_switch.hpp','CMakeLists.txt')]
    names+=['tests/'+n for n in ('matrixgame_native_font_windows.cpp','test_matrixgame_native_font.cpp','matrixgame_native_font_probe.cpp','probe_matrixgame_native_entry.cpp','test_matrixgame_map_bc1_boundary.cpp')]
    names+=['tools/'+n for n in ('matrixgame_font_checks.py','matrixgame_font_audit.py','matrixgame_font_map_inventory.py','matrixgame_font_boundary_checks.py')]
    result={'date':'2026-10-25','stage':'E2E-5.21 PARTIAL','parent':parent,
        'debug_font':'original MS Sans Serif 25, charset204; 222 original-face glyphs; real Deko atlas upload/draw/release/lost/reset/rebind implemented',
        'font_linking_outside_original_face':'UNSUPPORTED explicitly, no substitute font',
        'Switch_runtime':False,'native_Init_returned':False,'native_map_loaded':False,
        'next_known_source_boundary':'CGame::Init -> PrepareMap -> BuildTexUnions -> CBottomTextureUnion::MakeFromBitmap -> CTextureManaged::LoadFromBitmap(DXT1,6) -> native_texture::Decode',
        'next_exception':'native texture formatted decode conversion','boundary_hardware_observed':False,
        'results':results,'boundary':boundary,'map_inventory':inventory,
        'source_sha256':{n:sha(ROOT/n) for n in names},
        'protected':protected,'protected_files_verified':checked,'hardware_files_verified':len(hardware),
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'primary_NRO_verified_against_previous_fingerprint':primary,
        'root_cumulative_NRO_verified_against_documented_hash':previous['root_cumulative_NRO_verified_against_documented_hash'],
        'original_package_CFG_frozen_cache_unchanged':True,'original_Init_Draw_algorithms_unchanged':True,
        'broad_Windows_regression_rerun':False,'Windows_HAL_font_only':True,
        'CMake_configure_generate':'PASS','CMake_font_binary_object':'PASS; dependency core/material/vector nodes built; full engine CMake build NOT RUN',
        'NRO_created':False,'commit':False,'push':False,
        'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e521-font-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':list(results),'protected':checked,'hardware_inputs':len(hardware),'next':result['next_exception'],'Switch_runtime':False}))
if __name__=='__main__':main()

"""Audit native cursor/input software evidence. Does not execute Switch or create NRO."""
import json,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/cursor'
def main():
    results={}
    for kind in ('host','arm64','windows','sdk','windows-frame'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        if kind=='sdk':
            assert not j['strong_undefined'] and sha(BASE/kind/'native-entry.elf')==j['elf_sha256']
            for n,h in j['link_inputs_sha256'].items():assert sha(Path(n))==h,n
        elif kind=='windows-frame':
            assert j['runtime_exit']==0 and j['stdout'].count('original CGame::Init returned: ready=1')==2
            assert j['stdout'].count('actual SDK draws=1008')==2
            for n,h in j['snapshots'].items():assert sha(BASE/kind/n)==h,n
        else:
            assert 'native cursor/input CPU PASS OOM=3' in j['stdout']
            assert 'original_Form_Input=executed HID_GPU_NOT_RUN' in j['stdout']
            runtime=(BASE/kind/'runtime.log').read_text(encoding='utf-8')
            assert 'AddressSanitizer' not in runtime and 'runtime error:' not in runtime
        results[kind]=j
    previous=read(ROOT/'reports/matrixgame-e2e521-lifecycle-evidence.json')
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
    folder=ROOT/'build/matrixgame/e2e521/source/MatrixGame/src'
    layout=(folder/'MatrixMap.hpp').read_text()
    assert layout.index('CMatrixDebugInfo m_DI;')<layout.index('CMatrixCursor m_Cursor;')
    debug=(folder/'MatrixDebugInfo.cpp').read_text();assert 'df::Create(&m_Font)' in debug
    cursor=(folder/'MatrixCursor.cpp').read_text()
    assert 'm_OldCursor=m_CursorWindow->ExchangeCursor({});' in cursor
    assert 'for(int slot=0;slot<m_FramesCnt;++slot)m_CursorIcons[slot]={};' in cursor
    switch=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
    assert 'lifecycle_->cursor.Exchange(value.value)' in switch
    assert 'v.DrawCursor();v.commands.barrier' in switch
    assert 'override_state?*override_state:state.Read()' in switch
    font=(ROOT/'port/matrixgame/native-engine/debug_font_backend.cpp').read_text()
    assert 'debug font SDK creation unavailable on this backend' in font
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
    names=['port/matrixgame/native-engine/'+n for n in ('native_cursor.hpp','native_cursor.cpp',
        'native_input.hpp','native_input.cpp','native_input_engine.hpp','native_input_engine.cpp',
        'native_input_switch.hpp','native_input_switch.cpp','cursor_backend.cpp','cursor_backend.hpp',
        'platform_switch.cpp','platform_switch.hpp','CMakeLists.txt')]
    names+=['tools/matrixgame_cursor_overrides.py','tools/matrixgame_cursor_checks.py',
        'tools/matrixgame_cursor_windows_frame.py','tools/matrixgame_cursor_audit.py','tests/test_matrixgame_native_cursor_input.cpp']
    headers=[Path('C:/devkitPro/libnx/include/switch')/n for n in ('services/hid.h','runtime/pad.h','services/applet.h')]
    result={'date':'2026-10-25','stage':'E2E-5.21 PARTIAL','parent':parent,
        'ExchangeCursor':'real native icon registry/selection, null/previous identity and foreign/dead handle validation; source/SDK dependency closed',
        'cursor':'original atlas draw unchanged; original icon frames become RGBA images with hotspot; actual texture arena upload and final deko overlay wired',
        'input':'actual HID mouse/keyboard/touch/Npad route to original Input and CForm; source merge, focus releases, bounded typematic, baseline key restore',
        'cursor_unwind':'unpublished original icon array slots zeroed; native null destroy idempotent; Store Create OOM=3 tested',
        'cursor_overlay_render_state':'separate NativeRenderState override; original captured state not edited by overlay',
        'Switch_runtime':False,'native_Init_returned':False,'native_map_loaded':False,
        'next_known_source_boundary':'CGame::Init -> CMatrixMapLogic -> CMatrixMap::m_DI -> CMatrixDebugInfo::CMatrixDebugInfo -> debug_font::Create -> CreateFor',
        'next_exception':'original debug font Create: debug font SDK creation unavailable on this backend',
        'boundary_hardware_observed':False,
        'previous_boundary_correction':'m_DI is declared before m_Cursor; preceding lifecycle report incorrectly called cursor the first blocker',
        'remaining':['native debug font creation/draw/lost/reset with original font metrics and glyphs',
            'stored-state/offscreen/shadow/text/video/presentation paths used by original map/frame',
            'native Takt/Draw loop, owner bridge, complete controller UX and physical input tests',
            'physical Switch allocation/upload/Init/PrepareMap/frame/recreate/teardown'],
        'results':results,'source_sha256':{n:sha(ROOT/n) for n in names},
        'generated_cursor_sha256':sha(folder/'MatrixCursor.cpp'),
        'libnx_header_sha256':{str(p):sha(p) for p in headers},
        'artifacts':{p.relative_to(ROOT).as_posix():{'bytes':p.stat().st_size,'sha256':sha(p)} for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.json','.log','.elf','.exe','.o','.obj','.png')},
        'protected':protected,'protected_files_verified':checked,'hardware_files_verified':len(hardware),
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'primary_NRO_verified_against_previous_fingerprint':primary,
        'root_cumulative_NRO_verified_against_documented_hash':previous['root_cumulative_NRO_verified_against_documented_hash'],
        'original_package_CFG_frozen_cache_unchanged':True,'original_Init_Draw_algorithms_unchanged':True,
        'broad_regressions_rerun':False,'CMake_full_rebuild':False,'NRO_created':False,'commit':False,'push':False,
        'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e521-cursor-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':list(results),'protected':checked,'hardware_inputs':len(hardware),'next':'CMatrixDebugInfo constructor -> debug_font::CreateFor','Switch_runtime':False}))
if __name__=='__main__':main()

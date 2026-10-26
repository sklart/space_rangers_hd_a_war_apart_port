"""Audit native applet lifecycle evidence and protected inputs; no runtime reruns."""
import json,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/lifecycle'

def main():
    results={}
    for kind in ('host','arm64','windows','sdk'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        if kind=='sdk':
            assert not j['strong_undefined'] and sha(BASE/kind/'native-entry.elf')==j['elf_sha256']
            for n,h in j['link_inputs_sha256'].items():assert sha(Path(n))==h,n
        else:
            assert 'native applet lifecycle CPU PASS' in j['stdout']
            assert 'original_CForm IPC_MODEL_ONLY GPU_NOT_RUN' in j['stdout']
            runtime=(BASE/kind/'runtime.log').read_text(encoding='utf-8')
            assert 'AddressSanitizer' not in runtime and 'runtime error:' not in runtime
        results[kind]=j
    previous=read(ROOT/'reports/matrixgame-e2e521-state-evidence.json')
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
    game=(ROOT/'build/matrixgame/e2e521/source/MatrixGame/src/MatrixGame.cpp').read_text()
    cursor=(ROOT/'build/matrixgame/e2e521/source/MatrixGame/src/MatrixCursor.cpp').read_text()
    assert game.index('ReadyWindow().Update()')<game.index('CMatrixMapLogic;')<game.index('g_MatrixMap->RobotPreload()')<game.index('g_MatrixMap->PrepareMap(')
    assert 'm_OldCursor=m_CursorWindow->ExchangeCursor({});' in cursor
    switch=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
    assert 'SwitchWindow::Show(Visibility value){lifecycle_->session.Show' in switch
    assert 'SwitchWindow::Update(){lifecycle_->session.Update()' in switch
    assert switch.index('v.window.WaitForDrawable();')<switch.index('v.image_slot=v.queue.acquireImage')
    assert 'ExchangeCursor' not in switch
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
    names=['port/matrixgame/native-engine/'+n for n in ('platform_switch.cpp','platform_switch.hpp',
        'applet_window_lifecycle.cpp','applet_window_lifecycle.hpp','applet_engine_events.cpp','applet_engine_events.hpp','CMakeLists.txt')]
    names+=['tests/test_matrixgame_applet_window.cpp','tests/probe_matrixgame_native_entry.cpp',
        'tools/matrixgame_lifecycle_checks.py','tools/matrixgame_lifecycle_audit.py']
    artifacts={p.relative_to(ROOT).as_posix():{'bytes':p.stat().st_size,'sha256':sha(p)}
        for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.json','.log','.elf','.exe','.o','.obj')}
    sdk_headers=[Path('C:/devkitPro/libnx/include/switch')/n for n in ('services/applet.h','display/native_window.h','kernel/event.h')]
    result={'date':'2026-10-25','stage':'E2E-5.21 PARTIAL','parent':parent,
        'Show_Update':'implemented with actual libnx applet visibility and message processing; source/SDK dependency closed',
        'semantics':'explicit owner visibility baseline; borrowed services/NWindow/suspend policy unchanged; own hook removed; restore failure observable',
        'focus_exit':'original flags/KEEPALIVE and CForm SystemEvent; wait for foreground before new acquire; sticky exit unwinds',
        'extent_change':'explicit swapchain recreation blocker; not faked',
        'Switch_runtime':False,'native_Init_returned':False,'native_map_loaded':False,
        'next_known_source_boundary':'CGame::Init -> CMatrixMapLogic -> CMatrixMap member m_Cursor -> CMatrixCursor::CMatrixCursor -> Window::ExchangeCursor',
        'next_exception':'backend system cursor unsupported','boundary_hardware_observed':False,
        'remaining':['native cursor/input provider preserving original software/hardware cursor behavior',
            'stored-state/offscreen/shadow/font/video/presentation providers used by original map/frame',
            'native frame loop integration with Takt/Draw and owner return',
            'physical Switch allocation/upload/Init/PrepareMap/frame/recreate/teardown'],
        'results':results,'source_sha256':{n:sha(ROOT/n) for n in names},'artifacts':artifacts,
        'libnx_header_sha256':{str(p):sha(p) for p in sdk_headers},
        'protected':protected,'protected_files_verified':checked,'hardware_files_verified':len(hardware),
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'primary_NRO_verified_against_previous_fingerprint':primary,
        'root_cumulative_NRO_verified_against_documented_hash':previous['root_cumulative_NRO_verified_against_documented_hash'],
        'original_package_CFG_frozen_cache_unchanged':True,'original_Init_Draw_sources_unchanged':True,
        'broad_regressions_rerun':False,'CMake_full_rebuild':False,
        'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN',
        'manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e521-lifecycle-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':list(results),'protected':checked,'hardware_inputs':len(hardware),
        'next':'CMatrixCursor constructor -> Window::ExchangeCursor','Switch_runtime':False}))
if __name__=='__main__':main()

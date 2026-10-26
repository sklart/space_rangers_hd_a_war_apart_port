"""Final native state gate audit, without repeating runtime suites."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/state'
def main():
    results={}
    for kind in ('host','arm64','windows','sdk','windows-frame'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        if kind in ('host','arm64','windows'):
            assert 'restores=9 capture_OOM=2 COW_OOM=1 restore_allocations=0' in j['stdout']
            assert 'GPU_NOT_RUN' in j['stdout']
            runtime=(BASE/kind/'runtime.log').read_text(encoding='utf-8');assert 'AddressSanitizer' not in runtime and 'runtime error:' not in runtime
        elif kind=='sdk':
            assert not j['strong_undefined'] and sha(BASE/kind/'native-entry.elf')==j['elf_sha256']
            for n,h in j['link_inputs_sha256'].items():assert sha(Path(n))==h,n
        else:
            assert j['runtime_exit']==0 and j['stdout'].count('original CGame::Init returned: ready=1')==2
            assert j['stdout'].count('actual SDK draws=1008')==2
            for n,h in j['snapshots'].items():assert sha(BASE/kind/n)==h,n
        results[kind]=j
    previous=read(ROOT/'reports/matrixgame-e2e521-residency-evidence.json')
    protected=previous['protected'];checked=0
    for n,v in protected.items():
        if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
    for n,v in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
    primary=previous['primary_NRO_current_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
    package=ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg'
    assert sha(package)==previous['results']['host']['package_sha256']
    init=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
    for n,h in init['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    original=read(ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json')
    for n in ('port/matrixgame/native-engine/original_game.cpp','tools/matrixgame_original_game_overrides.py',
        'tools/matrixgame_skin_manager_overrides.py','tools/matrixgame_render_pipeline_overrides.py'):
        assert sha(ROOT/n)==original['source_and_artifact_sha256_at_audit'][n],n
    p=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
    assert 'DekoDevice::CaptureState(){return impl_->state.Capture();}' in p
    assert 'DekoDevice::OutstandingStateLeases()const{return impl_->state.Outstanding();}' in p
    assert 'const auto& saved_state=state.Read();' in p
    assert 'state.Frame(true)' in p and 'state.Frame(false)' in p
    assert 'void SwitchWindow::Show(Visibility){throw Unsupported("Switch visibility requires native applet lifecycle");}' in p
    game=(ROOT/'build/matrixgame/e2e521/source/MatrixGame/src/MatrixGame.cpp').read_text()
    assert 'ReadyWindow().Show(srhd_awa::matrixgame::platform3g::Visibility::Normal)' in game
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
    names=['port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-engine/native_device_state.hpp',
        'port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/platform_switch.hpp',
        'port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/native-core/buffer_resources.hpp',
        'port/matrixgame/native-engine/CMakeLists.txt','port/matrixgame/native-engine/init_runtime.cpp',
        'tests/test_matrixgame_native_state.cpp','tools/matrixgame_state_checks.py','tools/matrixgame_state_windows_frame.py',
        'tools/matrixgame_state_audit.py']
    artifacts={}
    for folder in BASE.iterdir():
        if folder.is_dir():
            for f in folder.iterdir():
                if f.is_file() and f.suffix in ('.elf','.exe','.o','.obj','.png','.log','.json'):
                    artifacts[f.relative_to(ROOT).as_posix()]={'bytes':f.stat().st_size,'sha256':sha(f)}
    result={'date':'2026-10-25','stage':'E2E-5.21 PARTIAL','parent':parent,
        'CaptureState':'implemented in native state store used by DekoDevice; source/SDK gate closed; physical invocation NOT RUN',
        'restore':'no allocation; reapply saved authoritative state and retained buffer bindings; actual SDK recording uses restored state next draw',
        'snapshot_scope':'all currently exposed mutable native state; fixed owned programs/targets unchanged; quiescent frame required',
        'resource_contents_rolled_back':False,'original_Init_Draw_sources_unchanged':True,
        'Switch_runtime':False,'native_Init_returned':False,'native_map_loaded':False,
        'next_known_source_boundary':'CGame::Init -> ReadyWindow().Show(Visibility::Normal) -> SwitchWindow::Show',
        'next_exception':'Switch visibility requires native applet lifecycle','boundary_hardware_observed':False,
        'remaining':['applet/window visibility/update lifecycle','original stored-state backend and video/font/cursor/input providers',
            'remaining texture/offscreen/surface/shadow paths','hardware allocation/upload/restore and original Init/PrepareMap/frame'],
        'results':results,'protected':protected,'protected_files_verified':checked,
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],'primary_NRO_verified_against_previous_fingerprint':primary,
        'root_cumulative_NRO_verified_against_documented_hash':previous['root_cumulative_NRO_verified_against_documented_hash'],
        'hardware_files_verified':len(hardware),'original_package_CFG_frozen_cache_unchanged':True,
        'source_sha256':{n:sha(ROOT/n) for n in names},'artifacts':artifacts,
        'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN',
        'manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e521-state-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':list(results),'protected':checked,'hardware_inputs':len(hardware),
        'next':'SwitchWindow::Show(Normal)','Switch_runtime':False}))
if __name__=='__main__':main()

"""Audit actual presentation evidence; SDK success never means Switch runtime."""
import hashlib,json,re,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e523-presentation'
def main():
    results={}
    for kind in ('host','arm64','windows','sdk','windows-frame','text-boundary'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        results[kind]=j
    for kind in ('host','arm64','windows'):
        assert 'CPU_DRIVER_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN' in results[kind]['stdout']
        if kind=='windows':assert '/UNDEBUG' in (BASE/'windows/build.cmd').read_text()
        else:assert '-UNDEBUG' in results[kind]['commands'][0]['command']
    assert any('-fsanitize=address,undefined' in str(c) for c in results['host']['commands'])
    s=results['windows-frame']['stdout'];assert s.count('original CGame::Init returned: ready=1')==2 and s.count('actual SDK draws=1008')==2
    for n,h in results['windows-frame']['snapshots'].items():assert sha(BASE/'windows-frame'/n)==h
    assert 'original Text::Render Top/mm Font.2Ranger negative guard: text SDK device unavailable PASS' in results['text-boundary']['stdout']
    sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
    for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
    cmake=read(BASE/'cmake-host/result.json');assert cmake['status']=='PASS' and all(c['exit']==0 for c in cmake['commands'])
    assert '100% tests passed, 0 tests failed out of 1' in (BASE/'cmake-host/ctest-final.log').read_text()
    flags=(BASE/'cmake-host/CMakeFiles/matrixgame-native-presentation-test.dir/flags.make').read_text();assert '-UNDEBUG' in flags and '-fsanitize=address,undefined' in flags
    generated={}
    reference=read(ROOT/'build/matrixgame/e2e522/repro-result.json')['files_sha256']
    for kind in ('sdk','cmake-host'):
        marker=read(BASE/kind/'source/native-engine-source.json');assert marker['files_sha256']==reference and len(reference)==252
        for n,h in marker['files_sha256'].items():assert sha(BASE/kind/'source'/n)==h,(kind,n)
        generated[kind]=marker['source_commit']
    previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
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
    git=r'C:\Program Files\Git\cmd\git.exe';upstream=ROOT/'upstream/matrixgame'
    upstream_head=subprocess.check_output([git,'rev-parse','HEAD'],cwd=upstream,text=True).strip()
    upstream_status=subprocess.check_output([git,'status','--porcelain','--untracked-files=all'],cwd=upstream,text=True)
    assert upstream_head=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not upstream_status

    names=['port/matrixgame/native-engine/'+n for n in ('native_presentation.hpp','platform_switch.cpp','platform_switch.hpp','native_device_state.cpp','native_device_state.hpp','native_texture.hpp','native_frame_scope.hpp','init_runtime.cpp','CMakeLists.txt')]
    names+=['tests/test_matrixgame_native_presentation.cpp','tests/test_matrixgame_native_text_boundary.cpp','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e523-presentation.md']
    names += [str(p.relative_to(ROOT)) for p in (ROOT/'tools').glob('matrixgame_presentation_*.py')]
    for n in names:
        text=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',s) or s.rstrip(' \t')!=s for s in text.splitlines()),n
    assert not list(BASE.rglob('*.nro'))
    git=r'C:\Program Files\Git\cmd\git.exe';p=subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
    result={'stage':'E2E-5.23 NATIVE PRESENTATION SOFTWARE PASS / FULL NATIVE INIT PARTIAL','parent':parent,'upstream_head':upstream_head,
        'results':results,'cmake':cmake,'generated_sources':generated,'source_sha256':{n:sha(ROOT/n) for n in names},
        'protected':protected,'protected_verified':checked,'hardware_inputs_verified':len(hardware),
        'primary_NRO':primary,'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE PENDING',
        'next_guard':'CGame::Init -> CInterface::Load(Top) -> SStateImages::SetStateText -> Text::Render -> text_backend::DeviceIdentity',
        'next_exception':'text SDK device unavailable','resource':'Top/mm/StateStaticLabel Font.2Ranger Menu; source analysis + original Text::Render negative execution',
        'display_reset_GPU_IPC_execution':False,'CPU_low_level_driver_is_model':True,'NRO_created':False,'commit':False,'push':False,
        'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e523-presentation-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'strong_undefined':len(sdk['strong_undefined']),'next':result['next_exception']}))
if __name__=='__main__':main()

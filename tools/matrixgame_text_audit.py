"""Strict native UI text evidence and protected-input audit. No hardware claims."""
import hashlib,json,re,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e524-text'
def main():
    results={}
    for kind in ('windows','host','arm64','sdk'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        for n,h in j['assets_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        results[kind]=j
    for kind in ('host','arm64'):
        assert 'glyph/layout goldens=17940;' in results[kind]['stdout']
        assert 'CPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN' in results[kind]['stdout']
        assert '-UNDEBUG' in results[kind]['commands'][0]['command']
    assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
    assert 'pixel/layout cases=1164 exact' in results['windows']['stdout']
    assert results['windows']['stdout'].count('pixel/layout cases=5592 exact')==3
    assert sha(BASE/'windows/font0.sfnt')==sha(ROOT/'upstream/matrixgame/MatrixGame/Rangers.ttf')
    assert sha(BASE/'windows/font1.sfnt')==sha(BASE/'windows/font2.sfnt')==sha(BASE/'windows/font3.sfnt')
    sdk=results['sdk'];assert not sdk['strong_undefined']
    assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
    for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
    cmake=read(BASE/'cmake-host/result.json');assert cmake['status']=='PASS' and all(c['exit']==0 for c in cmake['commands'])
    assert '100% tests passed, 0 tests failed out of 1' in (BASE/'cmake-host/ctest.log').read_text()
    flags=(BASE/'cmake-host/CMakeFiles/matrixgame-native-text-test.dir/flags.make').read_text();assert '-UNDEBUG' in flags and '-fsanitize=address,undefined' in flags
    reference=read(ROOT/'build/matrixgame/e2e522/repro-result.json')['files_sha256']
    for kind in ('sdk','cmake-host'):
        marker=read(BASE/kind/'source/native-engine-source.json');assert marker['files_sha256']==reference and len(reference)==252
        for n,h in reference.items():assert sha(BASE/kind/'source'/n)==h,(kind,n)
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

    names=['port/matrixgame/native-engine/'+n for n in ('native_text.hpp','native_text.cpp','native_text_backend.cpp','native_text_asset.cpp','text_backend.cpp','platform_switch.cpp','platform_switch.hpp','CMakeLists.txt')]
    names+=['tests/test_matrixgame_native_text.cpp','tests/test_matrixgame_native_text_boundary.cpp','tests/matrixgame_text_font_export.cpp','tools/matrixgame_text_checks.py','tools/matrixgame_text_cmake.py','tools/matrixgame_text_audit.py','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e524-text.md']
    for n in names:
        s=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',line) or line.rstrip()!=line for line in s.splitlines()),n
    p=subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
    assert not list(BASE.rglob('*.nro'))
    result={'stage':'E2E-5.24 ORIGINAL UI TEXT SOFTWARE PASS / FULL NATIVE INIT PARTIAL','parent':parent,'upstream_head':upstream_head,
        'results':results,'cmake':cmake,'source_sha256':{n:sha(ROOT/n) for n in names},
        'protected':protected,'protected_verified':checked,'hardware_inputs_verified':len(hardware),
        'primary_NRO':primary,'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE PENDING',
        'next_guard_source_trace':'CGame::Init -> CreatePoolDefaultResources(true) -> StaticPrepare2 -> RNeed(MR_ShadowProjTex) -> CVectorObject::CalcShadowTextureWOMat -> offscreen::GetTarget',
        'next_exception':'offscreen target query unavailable','next_guard_executed_as_part_of_native_Init':False,
        'following_blockers':['native shadow target/binding/readback/staging draw','CMinimap::RenderBackground render texture usage=1 and offscreen surfaces','StoredStateBackend during ReleasePoolDefaultResources/recreate'],
        'font_limit':'exact original face glyph coverage; outside-face font linking and complex script shaping remain unsupported',
        'CPU_low_level_upload_is_model':True,'NRO_created':False,'commit':False,'push':False,
        'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e524-text-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'strong_undefined':len(sdk['strong_undefined']),'next':result['next_exception']}))
if __name__=='__main__':main()

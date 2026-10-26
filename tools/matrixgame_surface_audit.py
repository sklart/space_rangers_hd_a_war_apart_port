"""Audit actual surface software/HAL/SDK evidence without pretending Switch runtime."""
import json,re,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e525-surfaces'
def main():
    results={}
    for kind in ('host','arm64','solid-host','solid-arm64','sdk','windows-frame','windows-format'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        results[kind]=j
    for kind in ('host','arm64'):
        assert 'CPU_DRIVER_MODEL_ONLY NO_GPU NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN' in results[kind]['stdout']
        assert '-UNDEBUG' in results[kind]['commands'][0]['command']
    for kind in ('solid-host','solid-arm64'):
        assert 'original CTexture::MakeSolidTexture exact 2x2' in results[kind]['stdout']
        assert 'CPU_UPLOAD_MODEL_ONLY NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN' in results[kind]['stdout']
    for kind in ('host','solid-host'):assert '-fsanitize=address,undefined' in results[kind]['commands'][0]['command']
    s=results['windows-frame']['stdout'];assert s.count('original CGame::Init returned: ready=1')==2 and s.count('actual SDK draws=1008')==2
    for n,h in results['windows-frame']['snapshots'].items():assert sha(BASE/'windows-frame'/n)==h
    assert 'requested=28 raw_check=00000000 D3DXCreateTexture=00000000 actual_format=28' in results['windows-format']['stdout']
    assert 'requested=51 raw_check=8876086a D3DXCreateTexture=00000000 actual_format=21' in results['windows-format']['stdout']
    sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
    for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
    cmake=read(BASE/'cmake-host/result.json');assert cmake['status']=='PASS' and all(c['exit']==0 for c in cmake['commands'])
    assert '100% tests passed, 0 tests failed out of 1' in (BASE/'cmake-host/ctest.log').read_text()
    flags=(BASE/'cmake-host/CMakeFiles/matrixgame-native-surfaces-test.dir/flags.make').read_text();assert '-UNDEBUG' in flags and '-fsanitize=address,undefined' in flags
    old=read(ROOT/'build/matrixgame/e2e524-text/sdk/source/native-engine-source.json')['files_sha256']
    reference=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(reference)==252
    changed=[n for n,h in reference.items() if old[n]!=h]
    assert set(changed)=={'MatrixLib/3G/Texture.cpp','MatrixLib/3G/OriginalUnmanagedTexture.cpp','MatrixLib/3G/OriginalShadowTexture.cpp'}
    for folder in ('sdk/source','cmake-host/source','repro-source'):
        marker=read(BASE/folder/'native-engine-source.json');assert marker['files_sha256']==reference,folder
        for n,h in reference.items():assert sha(BASE/folder/n)==h,(folder,n)
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

    names=['port/matrixgame/native-engine/'+n for n in ('native_surfaces.hpp','native_surfaces.cpp','offscreen_scope.hpp','offscreen_scope.cpp','offscreen_backend.cpp','platform_switch.cpp','platform_switch.hpp','CMakeLists.txt','shadow_texture_backend.cpp')]
    names+=['port/matrixgame/deko/texture_store.hpp','port/matrixgame/deko/texture_store.cpp','tools/matrixgame_unmanaged_texture_overrides.py','tools/matrixgame_shadow_texture_overrides.py','tests/test_matrixgame_native_surfaces.cpp','tests/test_matrixgame_original_solid_texture.cpp','tests/matrixgame_shadow_format_windows.cpp','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e525-surfaces.md']
    names+=[str(p.relative_to(ROOT)) for p in (ROOT/'tools').glob('matrixgame_surface_*.py')]
    for n in names:
        s=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',line) or line.rstrip()!=line for line in s.splitlines()),n
    p=subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
    assert not list(BASE.rglob('*.nro'))
    result={'stage':'E2E-5.25 STANDALONE OFFSCREEN SOFTWARE PASS / FULL NATIVE INIT PARTIAL','parent':parent,'upstream_head':upstream_head,
        'results':results,'cmake':cmake,'source_sha256':{n:sha(ROOT/n) for n in names},'generated_changed':changed,
        'protected':protected,'protected_verified':checked,'hardware_inputs_verified':len(hardware),'primary_NRO':primary,
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
        'next_guard':'ShadowProjBuildFull/BuildTextureList -> CVectorObject::CalcShadowTextureWOMat(tex_to_update) -> shadow_texture::CreateRenderTexture(A8,usage=1,pool=0)',
        'next_exception':'shadow render texture SDK unavailable','next_guard_executed_as_part_of_native_Init':False,
        'A8_RGBA_substitution_justified':False,'Windows_HAL_original_A8_actual_format':28,
        'following_blockers':['native render texture/surface import','system-memory surface copy/Readback','minimap render texture','StoredStateBackend during ReleasePoolDefaultResources/recreate'],
        'CPU_low_level_driver_is_model':True,'NRO_created':False,'commit':False,'push':False,
        'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e525-surfaces-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'strong_undefined':len(sdk['strong_undefined']),'next':result['next_exception']}))
if __name__=='__main__':main()

"""Audit E2E-5.26 evidence; no native Init/hardware inference."""
import json,re,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e526-render-textures'
def main():
    results={}
    for kind in ('render-host','render-arm64','sdk','windows-alpha'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        results[kind]=j
    for kind in ('render-host','render-arm64'):
        assert 'CPU_DRIVER_MODEL_ONLY NO_GPU NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN' in results[kind]['stdout']
        assert '-UNDEBUG' in results[kind]['commands'][0]['command']
    assert '-fsanitize=address,undefined' in results['render-host']['commands'][0]['command']
    assert '9184 cases PASS maximum delta=0' in results['windows-alpha']['stdout']
    material=read(BASE/'material-reference/windows/result.json');assert material['status']=='PASS' and all(c['exit']==0 for c in material['commands'])
    for n,h in material['input_sha256'].items():assert sha(ROOT/n)==h,n
    assert 'GLSL_mismatches=0' in material['stdout'] and 'D3D9_differential=1476 mismatches=0' in material['stdout']
    shaders=read(BASE/'sdk/material-shaders/shader-build.json')
    for stage,j in shaders.items():
        assert sha(ROOT/('port/matrixgame/deko/material.'+stage+'.glsl'))==j['source_sha256']
        assert sha(BASE/('sdk/material-shaders/material.'+stage+'.dksh'))==j['dksh_sha256']
    sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
    for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
    reference=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(reference)==252
    assert reference==read(ROOT/'build/matrixgame/e2e525-surfaces/sdk/source/native-engine-source.json')['files_sha256']
    for n,h in reference.items():assert sha(BASE/'sdk/source'/n)==h,n
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


    names=sorted(set(n for j in results.values() for n in j['source_sha256']))
    names+=['tools/matrixgame_render_texture_audit.py','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e526-render-textures.md']
    for n in names:
        s=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',line) or line.rstrip()!=line for line in s.splitlines()),n
    p=subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
    assert not list(BASE.rglob('*.nro'))
    result={'stage':'E2E-5.26 RENDER TEXTURE/SURFACE SOFTWARE PASS / FULL NATIVE INIT PARTIAL','parent':parent,'upstream_head':upstream_head,
        'results':results,'windows_material_reference':material,'shaders':shaders,'source_sha256':{n:sha(ROOT/n) for n in names},'generated_changed':[],
        'protected':protected,'protected_verified':checked,'hardware_inputs_verified':len(hardware),'primary_NRO':primary,
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
        'next_source_traced_guard':'CMatrixMap::CreatePoolDefaultResources(true) -> CMinimap::RenderBackground -> PathToOutputFiles -> output_paths::PersonalLocation',
        'next_exception':'native output personal-folder mapping unavailable','next_guard_executed_as_part_of_native_Init':False,
        'following_blockers':['CMinimap target/state/scene exception cleanup','native StoredStateBackend in CMatrixMap::ReleasePoolDefaultResources','actual native Init/PrepareMap/GPU first frame hardware validation'],
        'CPU_low_level_driver_is_model':True,'NRO_created':False,'commit':False,'push':False,
        'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e526-render-textures-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'strong_undefined':len(sdk['strong_undefined']),'next':result['next_exception']}))
if __name__=='__main__':main()

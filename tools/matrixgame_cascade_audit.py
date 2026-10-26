"""E2E-5.21 alpha/null closure and native shader/provider integration evidence."""
import hashlib,json,re,subprocess
from pathlib import Path
from matrixgame_vector_object_overrides import function
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/cascade'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    results={};artifacts={}
    for kind in ('host','arm64','windows','sdk','cmake-windows','cmake-sdk'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for name,h in j['input_sha256'].items():assert digest(ROOT/name)==h,(kind,name)
        if kind in ('host','arm64','windows'):
            assert 'accepted=1476 disabled_alpha=0 null_texture=0' in j['stdout']
            assert 'mismatches=0' in j['stdout'] and 'GLSL_mismatches=0' in j['stdout']
            assert 'fixed vertex cases=84' in j['stdout']
            if kind=='windows':
                assert 'D3D9_differential=1476' in j['stdout'] and 'GLSL_differential=1476' in j['stdout']
                assert 'GLSL_feedback=84 D3D9_ProcessVertices=80' in j['stdout']
            else:
                assert 'no D3D identities; GPU NOT RUN' in j['routing_stdout']
                for log in ('runtime.log','routing-runtime.log'):
                    text=(BASE/kind/log).read_text(encoding='utf-8');assert 'AddressSanitizer' not in text and 'runtime error:' not in text
        results[kind]=j
    native=read(BASE/'native-entry/result.json');assert native['status']=='PASS' and not native['runtime_executed'] and not native['strong_undefined']
    for name,h in native['inputs_sha256'].items():assert digest(Path(name))==h,name
    assert digest(BASE/'native-entry/native-entry.elf')==native['elf_sha256']
    windows=read(BASE/'windows-engine/result.json');assert windows['status']=='PASS' and windows['runtime_exit']==0
    assert windows['stdout'].count('original CGame::Init returned: ready=1')==2
    assert windows['stdout'].count('actual SDK draws=1008')==2
    for name,h in windows['snapshots'].items():assert digest(BASE/'windows-engine'/name)==h
    old=read(ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json')
    for name in ('port/matrixgame/native-engine/original_game.cpp','tools/matrixgame_original_game_overrides.py',
                 'tools/matrixgame_skin_manager_overrides.py','tools/matrixgame_render_pipeline_overrides.py'):
        assert digest(ROOT/name)==old['source_and_artifact_sha256_at_audit'][name],name
    protected=old['protected']
    for name,entry in protected.items():
        if entry.get('status')!='UNAVAILABLE':assert digest(Path(name))==entry['sha256'],name
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for name,entry in hardware.items():
        p=ROOT/'logs/2026.10.23'/name;assert p.stat().st_size==entry['bytes'] and digest(p)==entry['sha256'],name
    inputs=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
    assert digest(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')==inputs['package_sha256']
    for name,h in inputs['source_cfg_sha256'].items():assert digest(ROOT/'upstream/matrixgame/MatrixGame/CFG'/name)==h
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert digest(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    provider=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text(encoding='utf-8')
    caps=function(provider,'DeviceCaps DekoDevice::ReadCaps()')
    assert 'VertexReady()' in caps and 'return {false,8,16,1,8}' in caps
    assert 'throw Unsupported("deko texture residency budget not negotiated")' in provider
    fragment=(ROOT/'port/matrixgame/deko/material.frag.glsl').read_text();vertex=(ROOT/'port/matrixgame/deko/material.vert.glsl').read_text()
    assert 'images[8]' in fragment and 'stages[8]' in fragment and 'inputUv[8]' in vertex
    for stage in ('frag','vert'):
        meta=read(BASE/'sdk/shader-build.json')[stage]
        assert digest(ROOT/f'port/matrixgame/deko/material.{stage}.glsl')==meta['source_sha256']
        assert digest(BASE/f'sdk/material.{stage}.dksh')==meta['dksh_sha256']
    for folder in (ROOT/'port/matrixgame/deko',ROOT/'port/matrixgame/native-engine',BASE):
        for p in folder.rglob('*'):
            if p.is_file() and p.suffix in ('.cpp','.hpp','.glsl','.cmake','.dksh','.elf','.o','.obj','.a','.lib','.exe','.png','.json','.log'):
                artifacts[p.relative_to(ROOT).as_posix()]={'bytes':p.stat().st_size,'sha256':digest(p)}
    for p in (ROOT/'tools').glob('matrixgame_*cascade*.py'):artifacts[p.relative_to(ROOT).as_posix()]={'bytes':p.stat().st_size,'sha256':digest(p)}
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==old['parent']
    result={'date':'2026-10-25','parent':parent,'stage':'E2E-5.21 PARTIAL',
        'original_209_alpha_null_cases':'covered with zero rejections/mismatches; four input datasets',
        'material_D3D9_GPU_comparisons':1476,'same_fragment_GLSL_host_GPU_comparisons':1476,
        'fixed_vertex_GLSL_feedback_comparisons':84,'D3D9_ProcessVertices_comparisons':80,
        'ReadCaps':'implemented and SDK-linked; runtime checks actual initialized shader pair; Switch invocation NOT RUN',
        'shader_caps':{'texture_stages':8,'simultaneous_textures':8,'max_anisotropy':16,'actual_swapchains':1,'calibrate_gamma':False},
        'NativeGraphics_map_and_VO_submission':'connected in source and original Init/frame ELF; actual native draw/acquire/present NOT RUN',
        'next_known_source_boundary':'CGame::Init -> EnterPlatform -> EnterNativePlatform -> InitAsNative -> DekoDevice::AvailableTextureBytes',
        'boundary_hardware_observed':False,'Switch_runtime':False,'native_map_loaded':False,'native_Init_returned':False,
        'remaining_for_first_map':['actual texture residency/allocation budget and original texture resource backend',
            'device state capture/restore and lifecycle','native video/window/font/cursor/input providers',
            'offscreen/raster/stencil/shadow integrations and native first-frame execution'],
        'hardware_allocation_alignment_verified':False,'main_game_P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN',
        'manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN','NRO_created':False,'commit':False,'push':False,
        'results':results,'native_entry_link':native,'Windows_original_Init_frame_regression':windows,
        'original_Init_Draw_sources_unchanged':True,'protected':protected,'hardware_files_verified':len(hardware),
        'original_package_CFG_frozen_cache_unchanged':True,'source_and_artifact_sha256':artifacts}
    (ROOT/'reports/matrixgame-e2e521-cascade-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'alpha_null':'zero pending cases in tested matrix','D3D9_and_GLSL':1476,
        'ReadCaps':'SDK source/link closed; Switch NOT RUN','next':'AvailableTextureBytes','protected':15,'hardware_inputs':len(hardware)},ensure_ascii=False))
if __name__=='__main__':main()

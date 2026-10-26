"""Verify final residency evidence and protected inputs; no tests rerun."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/residency'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
    results={}
    for kind in ('host','arm64','sdk','windows'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        for cmd in j['commands']:assert cmd['exit']==0,(kind,cmd['label'])
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        for n,h in j.get('inputs',{}).items():assert sha(ROOT/n)==h,(kind,n)
        if kind!='sdk':assert 'allocator_steps=4000 formats=12 decoded=8' in j['stdout'] and 'GPU_NOT_RUN' in j['stdout']
        else:
            assert not j['strong_undefined']
            for n,h in j['link_inputs_sha256'].items():assert sha(Path(n))==h,n
            assert sha(BASE/kind/'native-entry.elf')==j['elf_sha256']
        results[kind]=j
    cmake=read(BASE/'cmake-sdk/result.json');assert cmake['status']=='PASS'
    # This material target's configure/build is independent of the provider.
    # Record its executed commands without treating its broad legacy manifest
    # as proof that all listed provider source files were compiled by CMake.
    assert all(c['exit']==0 for c in cmake['commands'])
    previous=read(ROOT/'reports/matrixgame-e2e521-cascade-evidence.json')
    for n,v in previous['protected'].items():
        if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n
    protected_verified=sum(v.get('status')!='UNAVAILABLE' for v in previous['protected'].values())
    provenance=read(ROOT/'reports/e2e5/e2e510-artifact-provenance.json');extra_frozen={}
    for v in provenance['final'].values():
        p=Path(v['path']);assert p.stat().st_size==v['bytes'] and sha(p)==v['sha256'].lower(),str(p)
        extra_frozen[str(p)]={'bytes':p.stat().st_size,'sha256':sha(p)}
    cumulative=ROOT/'Switch-E2E5-TEST.nro';cumulative_hash='b1ef8236adc8cda13229e3f2d72851928d05214c4102758207bd188b6ade874c'
    assert sha(cumulative)==cumulative_hash
    assert cumulative_hash.upper() in (ROOT/'docs/e2e5-coverage-matrix.md').read_text(encoding='utf-8')
    primary=ROOT/'port/switch/Space Rangers HD - A War Apart.nro'
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
    original=read(ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json')
    for n in ('port/matrixgame/native-engine/original_game.cpp','tools/matrixgame_original_game_overrides.py',
              'tools/matrixgame_skin_manager_overrides.py','tools/matrixgame_render_pipeline_overrides.py'):
        assert sha(ROOT/n)==original['source_and_artifact_sha256_at_audit'][n],n
    inputs=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
    assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')==inputs['package_sha256']
    for n,h in inputs['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    provider=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
    assert 'return impl_->texture_store->Available()' in provider
    assert 'deko texture residency budget not negotiated' not in provider
    assert 'original native texture residency/upload backend not connected' not in provider
    assert 'CaptureState(' not in provider
    attach=(ROOT/'port/matrixgame/native-engine/init_runtime.cpp').read_text();assert 'device_snapshot_=device.CaptureState();' in attach
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
    sources={}
    for pattern in ('port/matrixgame/native-engine/*texture*.*','port/matrixgame/deko/texture_store.*',
                    'port/matrixgame/native-engine/platform_switch.*','tools/matrixgame_residency*.py',
                    'tests/test_matrixgame_texture_residency.cpp'):
        for p in ROOT.glob(pattern):sources[p.relative_to(ROOT).as_posix()]=sha(p)
    for n in ('port/matrixgame/native-engine/CMakeLists.txt','port/matrixgame/deko/materials.cmake',
              'port/matrixgame/native-core/texture_asset.cpp','port/matrixgame/native-core/texture_mips.cpp'):
        sources[n]=sha(ROOT/n)
    report={'stage':'E2E-5.21 PARTIAL','parent':parent,'results':results,'material_CMake_SDK':cmake['commands'],
        'AvailableTextureBytes':'source/SDK implemented; largest contiguous free range of successfully allocated image-capable arena',
        'requested_default_arena_bytes':64*1024*1024,'Switch_arena_allocation_executed':False,
        'native_texture_backend':'original level bytes/locks/decode -> real Deko image/mip transfer/descriptors; native GPU NOT RUN',
        'CPU_upload_callback':'explicit fixture model only; never GPU success',
        'CPU_bridge_OOM_HRESULT_test':'host and ARM64; Windows real D3D bridge branches compile only',
        'native_Init_returned':False,'native_map_loaded':False,'native_frame_executed':False,
        'next_known_source_boundary':'CGame::Init -> EnterPlatform -> EnterNativePlatform -> AttachDeviceImpl -> Device::CaptureState',
        'next_boundary_hardware_observed':False,'remaining':['native state capture/restore and original sample-state backend',
            'video/window/font/cursor/input providers','offscreen surfaces/readback/shadows and required formatted conversions',
            'physical arena allocation/upload/sampling/lifetime proof; original Init/PrepareMap/frame'],
        'original_Init_Draw_sources_unchanged':True,'protected':previous['protected'],'hardware_files_verified':len(hardware),
        'protected_files_verified':protected_verified,'extra_frozen_artifacts_verified':extra_frozen,
        'root_cumulative_NRO_verified_against_documented_hash':cumulative_hash,
        'primary_NRO_current_fingerprint':{'path':str(primary),'bytes':primary.stat().st_size,'sha256':sha(primary),
            'mtime_ns':primary.stat().st_mtime_ns,'scope':'current fingerprint; no earlier primary-NRO baseline comparison claimed'},
        'package_CFG_frozen_cache_unchanged':True,'source_sha256':sources,
        'SDK_headers_sha256':{n:sha(Path(n)) for n in ('C:/devkitPro/libnx/include/deko3d.h','C:/devkitPro/libnx/include/deko3d.hpp')},
        'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN',
        'manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e521-residency-evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':report['stage'],'profiles':list(results),'protected_verified':protected_verified,
        'hardware_files':len(hardware),'next':'Device::CaptureState','Switch_runtime':False}))
if __name__=='__main__':main()

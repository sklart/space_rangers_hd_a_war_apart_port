"""Audit completed E2E-5.20 runs, source fidelity, artifacts and protected inputs."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib
from matrixgame_engine_prepare import prepare_engine
from matrixgame_vector_object_overrides import function

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e520'
OLD=ROOT/'build/matrixgame/e2e519'

def read(path):return json.loads(path.read_text(encoding='utf-8'))

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(4*1024*1024),b''):h.update(block)
    return h.hexdigest()

def png(path):
    data=path.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n'
    offset=8;compressed=[];extent=None
    while offset<len(data):
        size=struct.unpack_from('>I',data,offset)[0];kind=data[offset+4:offset+8]
        body=data[offset+8:offset+8+size]
        if kind==b'IHDR':extent=struct.unpack_from('>II',body)
        if kind==b'IDAT':compressed.append(body)
        offset+=size+12
    assert extent==(800,600)
    return {'sha256':digest(path),'extent':extent,
            'decoded_scanlines_sha256':hashlib.sha256(zlib.decompress(b''.join(compressed))).hexdigest()}

def main():
    artifacts={}
    def capture(path):
        assert path.is_file(),path
        artifacts[path.relative_to(ROOT).as_posix()]=digest(path)
    results={}
    windows=read(BASE/'windows/result.json');results['windows']=windows
    assert windows['runtime_exit']==0 and all(x==0 for x in windows['results'].values())
    out,err=windows['stdout'],windows['stderr']
    assert 'exception code=c0000005' not in out
    assert 'Failed to find font resource' not in err
    assert 'HAL CreateDevice HRESULT=00000000' in out
    assert 'actual DEFAULT-resource Reset failure/release/retry PASS' in out
    assert 'actual SDK/CPU staging bind/unbind and owner cleanup PASS' in out
    assert 'actual D3D9 Reset 640x480 HRESULT=8876086c' in err
    for marker in ('original CGame::Init returned: ready=1','actual client after Init 800x600',
                   'actual map framebuffer readback: 800x600 distinct_rgb=154038',
                   'original frame returned; actual SDK draws=1008 indexed=1006 primitives=111812',
                   'actual gamma/window readback restored; device/timer leases empty'):
        assert out.count(marker)==2,marker
    for method in ('BeforeDraw','DrawObjects','DrawLandscape','DrawLandscapeSurfaces','DrawWater','DrawShadows','DrawEffects','Draw'):
        assert err.count('original frame component: CMatrixMap::'+method+'\n')==2,method
    assert err.count('PrepareMap complete borrowed_skin_objects=9 map_size=276x285 groups=28x29')==2
    assert err.count('actual D3D9 Reset 800x600 HRESULT=00000000')==2
    assert err.count('actual D3D9 Reset 640x480 HRESULT=00000000')==3  # Negative test retry plus two game restores.
    snapshots={}
    for name in ('map-frame-0.png','map-frame-1.png'):
        path=BASE/'windows'/name;snapshots[path.relative_to(ROOT).as_posix()]=png(path);capture(path)

    for kind in ('host','arm64','sdk'):
        folder=BASE/('checks-'+kind);value=read(folder/'result.json')
        assert all(x==0 for x in value['results'].values()),kind
        assert value['runtime_executed']==(kind!='sdk')
        if kind=='sdk':
            assert not value['strong_undefined'] and not value['full_game_sdk_link']
            assert all('compile_'+unit in value['results'] for unit in ('owner','map_device','prepare','map','frame'))
        else:
            assert 'cleanup PASS; GPU NOT RUN' in (folder/'runtime.log').read_text()
        results[kind]=value
    # Match the tested generated view to a fresh disposable generation.
    source=BASE/'source';fresh=BASE/'audit-source';prepare_engine(fresh)
    generated={}
    for path in source.rglob('*'):
        if path.is_file() and path.suffix in ('.cpp','.h','.hpp','.inl','.inc'):
            name=path.relative_to(source).as_posix()
            assert digest(path)==digest(fresh/name),name
            generated[name]=digest(path)
    # Init/Deinit themselves retain the complete E2E-5.19 algorithm.
    previous=(OLD/'skin-borrow-source/MatrixGame/src/MatrixGame.cpp').read_text(encoding='utf-8')
    game=(source/'MatrixGame/src/MatrixGame.cpp').read_text(encoding='utf-8')
    for signature in ('void CGame::Init(','void CGame::Deinit(void)'):
        assert function(game,signature)==function(previous,signature),signature
    # Inverse translation of the full original form Draw must exactly match.
    frame=function((source/'MatrixGame/src/OriginalWindowsFrame.cpp').read_text(), 'void DrawOriginalWindowsFrame(')
    frame=frame.replace('void DrawOriginalWindowsFrame(void (*before_present)())','void CFormMatrixGame::Draw(void)')
    frame=frame.replace('\n    if(before_present)before_present();','')
    frame=frame.replace('frame_surface::Clear(','g_D3DD->Clear(')
    frame=frame.replace('frame_surface::BeginScene()','g_D3DD->BeginScene()')
    frame=frame.replace('frame_surface::EndScene()','g_D3DD->EndScene()')
    frame=frame.replace('srhd_awa::matrixgame::transition::Present()','g_D3DD->Present(NULL, NULL, NULL, NULL)')
    frame=frame.replace('2u | 4u | 1u','D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL | D3DCLEAR_TARGET')
    frame=frame.replace('2u | 4u','D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL')
    frame=frame.replace('NULL, 2u,','NULL, D3DCLEAR_ZBUFFER,').replace('ClearRects_Get(), 2u,','ClearRects_Get(), D3DCLEAR_ZBUFFER,')
    frame=frame.replace('0xffff00ffu','D3DCOLOR_XRGB(255, 0, 255)')
    original=(ROOT/'upstream/matrixgame/MatrixGame/src/MatrixFormGame.cpp').read_text(encoding='utf-8')
    assert frame==function(original,'void CFormMatrixGame::Draw(void)'), 'Original frame algorithm drift'
    for name in ('VectorObjectCpu.hpp','VectorObjectShared.hpp'):
        assert digest(source/'portable-core'/name)==digest(OLD/'skin-borrow-source/portable-core'/name)

    # Record compiled sources, live headers, runners and exact linked archives.
    for folder in (ROOT/'port/matrixgame/native-core',ROOT/'port/matrixgame/native-engine'):
        for path in folder.rglob('*'):
            if path.is_file() and path.suffix in ('.cpp','.hpp','.inc','.txt'):capture(path)
    for path in (ROOT/'tools').glob('matrixgame_*.py'):capture(path)
    for path in (ROOT/'tests').glob('*matrixgame*frame*'):capture(path)
    capture(ROOT/'tests/test_matrixgame_staging_draw.cpp')
    capture(ROOT/'upstream/matrixgame/MatrixGame/Rangers.ttf')
    for folder in ('windows','checks-host','checks-arm64','checks-sdk'):
        for path in (BASE/folder).iterdir():
            if path.is_file() and (path.suffix in ('.json','.o','.obj','.exe','.elf','.map','.res') or path.name=='staging-draw'):
                capture(path)
    for command in windows['commands'].values():
        if not isinstance(command,str):continue
        for name in re.findall(r'"([^"\n]+\.(?:lib|obj|res))"',command):
            path=Path(name)
            if not path.is_absolute():path=OLD/'original-init-windows'/path
            if path.is_file() and path.is_relative_to(ROOT):capture(path)
    cmake=OLD/'original-init-windows'
    for name in ('matrixgame-native-bootstrap.vcxproj','matrixgame-windows-game.vcxproj'):
        capture(cmake/name)
    project=(cmake/'matrixgame-windows-game.vcxproj').read_text(encoding='utf-8-sig')
    assert 'OriginalWindowsFrame.cpp' in project and 'matrixgame_windows_frame.rc' in project
    # Preserve exact protected inputs and frozen GI source evidence.
    baseline=read(ROOT/'reports/matrixgame-e2e519-original-init-evidence.json')
    protected={}
    for name,expected in baseline['protected'].items():
        if expected=='UNAVAILABLE':protected[name]={'status':'UNAVAILABLE'};continue
        assert digest(Path(name))==expected,name
        protected[name]={'sha256':expected,'unchanged':True}
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for name,value in hardware.items():
        path=ROOT/'logs/2026.10.23'/name
        assert path.stat().st_size==value['bytes'] and digest(path)==value['sha256'],name
    data=read(OLD/'original-init-host/result.json')
    assert digest(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')==data['package_sha256']
    for name,expected in data['source_cfg_sha256'].items():assert digest(ROOT/'upstream/matrixgame/MatrixGame/CFG'/name)==expected
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json')
    assert digest(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    manifest=read(ROOT/'build/e2e516/hardware-e2e516-03651ce/source-sha256.json')
    for call in frozen['evict_calls']:assert digest(ROOT/call['file'])==manifest[call['file']]
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    report={
        'date':'2026-10-25','parent':parent,'stage':'E2E-5.20 WINDOWS HAL PASS; SWITCH PARTIAL',
        'original_Init_returned':True,'original_frame_returned':True,'cycles':2,
        'map':[276,285],'groups':[28,29],'client_viewport_color_depth_extent':[800,600],
        'tracked_draws_per_frame':{'calls':1008,'indexed':1006,'primitives':111812},
        'snapshots':snapshots,'results':results,'generated_source_sha256':generated,
        'Init_Deinit_algorithms_equal_to_previous_full_candidate':True,
        'form_Draw_inverse_translation_equals_original':True,
        'source_and_artifact_sha256_at_audit':artifacts,'protected':protected,
        'hardware_files_verified':len(hardware),'original_package_and_CFG_unchanged':True,
        'Switch_hardware_executed':False,'GPU_alignment_hardware_verified':False,
        'full_game_loop_executed':False,'battle_outcome_verified':False,
        'first_remaining_Windows_Init_or_frame_blocker':None,
        'first_Switch_integration_boundary':'native typed entry and actual Window/Device/display lifecycle; legacy entry requires D3D identities',
        'main_game_P0':'OPEN; GI screen causality, manual-publish ENOENT, Switch hangs not closed',
        'push':False,'commit':False,
    }
    target=ROOT/'reports/matrixgame-e2e520-first-frame-evidence.json'
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':report['stage'],'cycles':2,'snapshots':snapshots,
        'protected_verified':sum(x.get('unchanged',False) for x in protected.values()),
        'hardware_files_verified':len(hardware)},ensure_ascii=False))

if __name__=='__main__':main()

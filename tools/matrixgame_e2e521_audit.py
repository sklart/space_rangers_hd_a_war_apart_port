"""Verify native-entry evidence without treating SDK linkage as Switch execution."""
import json,re,subprocess
from pathlib import Path
from matrixgame_e2e520_audit import digest,png
from matrixgame_engine_prepare import prepare_engine
from matrixgame_vector_object_overrides import function
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521'

def read(path):return json.loads(path.read_text(encoding='utf-8'))

def main():
    results={};files={}
    def capture(path):
        assert path.is_file(),path
        files[path.relative_to(ROOT).as_posix()]=digest(path)
    for kind in ('host','arm64','sdk'):
        folder=BASE/kind;j=read(folder/'result.json');results[kind]=j
        assert all(v==0 for v in j['results'].values()),kind
        if kind=='sdk':
            assert not j['runtime_executed'] and not j['strong_undefined']
            assert j['results']['compile_frame']==j['results']['compile_visibility']==0
        else:
            assert j['runtime_executed'] and 'two original cleanup cycles PASS' in j['stdout']
            assert j['stderr'].count('NATIVE_MATERIAL_CAPS_PENDING')==2
            assert j['stderr'].count('statics=1 heap=1 data=1 cache=1')==2
            assert 'AddressSanitizer' not in j['stderr'] and 'runtime error:' not in j['stderr']
        for path in folder.iterdir():
            if path.is_file() and (path.suffix in ('.o','.elf','.json') or path.name=='native-entry'):capture(path)
        for cmd in j['commands']:
            if '-c' in cmd:
                assert [x for x in cmd if x in ('-DNDEBUG','-UNDEBUG')][-1]=='-UNDEBUG'
            for name in cmd:
                if not name.endswith(('.a','.o')):continue
                path=Path(name.replace('/mnt/d/','D:/'))
                if not path.is_absolute():path=ROOT/'build/matrixgame/e2e519'/('vector-group-fresh-'+kind)/path
                if path.is_file() and path.is_relative_to(ROOT):capture(path)
    nm=subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',str(BASE/'sdk/native-entry.elf')],text=True)
    symbols={}
    for name in ('CGame::Init(','CMatrixMap::Draw()','CMatrixMap::CalcMapGroupVisibility()',
                 'DrawOriginalWindowsFrame(', 'NativeInitRuntime::InitializeNativeGame(',
                 'dkDeviceCreate','dkQueueCreate','dkSwapchainCreate'):
        matches=[x for x in nm.splitlines() if name in x];assert matches,name
        symbols[name]=matches[:3]
    windows=read(BASE/'windows/result.json');results['windows']=windows
    assert windows['runtime_exit']==0 and all(x==0 for x in windows['results'].values())
    for marker in ('original CGame::Init returned: ready=1','original frame returned; actual SDK draws=1008',
                   'actual gamma/window readback restored; device/timer leases empty'):
        assert windows['stdout'].count(marker)==2,marker
    previous=read(ROOT/'reports/matrixgame-e2e520-first-frame-evidence.json')
    expected=next(iter(previous['snapshots'].values()))['sha256']
    snapshots={}
    for name in ('map-frame-0.png','map-frame-1.png'):
        path=BASE/'windows'/name;snapshots[name]=png(path)
        assert snapshots[name]['sha256']==expected;capture(path)
    for path in (BASE/'windows').iterdir():
        if path.is_file() and path.suffix in ('.obj','.exe','.map','.res','.json'):capture(path)
    for cmd in windows['commands'].values():
        if not isinstance(cmd,str):continue
        for name in re.findall(r'"([^"\n]+\.(?:lib|obj|res))"',cmd):
            path=Path(name)
            if not path.is_absolute():path=ROOT/'build/matrixgame/e2e519/original-init-windows'/path
            if path.is_file() and path.is_relative_to(ROOT):capture(path)
    source=BASE/'source';fresh=BASE/'audit-source';prepare_engine(fresh)
    generated={}
    for path in source.rglob('*'):
        if path.is_file() and path.suffix in ('.cpp','.h','.hpp','.inl','.inc'):
            relative=path.relative_to(source).as_posix()
            assert digest(path)==digest(fresh/relative),relative
            generated[relative]=digest(path)
    old_source=ROOT/'build/matrixgame/e2e520/audit-source'
    before=(old_source/'MatrixGame/src/MatrixGame.cpp').read_text(encoding='utf-8')
    after=(source/'MatrixGame/src/MatrixGame.cpp').read_text(encoding='utf-8')
    after=after.replace('    original_game_owner::EnterPlatform(','    L3GInitAsDLL(',1)
    for signature in ('void CGame::Init(','void CGame::Deinit(void)'):
        assert function(before,signature)==function(after,signature),signature
    assert digest(source/'MatrixGame/src/OriginalWindowsFrame.cpp')==digest(old_source/'MatrixGame/src/OriginalWindowsFrame.cpp')
    for folder in ('native-core','native-engine'):
        for path in (ROOT/'port/matrixgame'/folder).rglob('*'):
            if path.is_file() and path.suffix in ('.cpp','.hpp','.inc','.txt'):capture(path)
    for path in (ROOT/'tools').glob('matrixgame_*.py'):capture(path)
    for name in ('tests/test_matrixgame_native_entry.cpp','tests/probe_matrixgame_native_entry.cpp'):
        capture(ROOT/name)
    ninja=BASE/'cmake-sdk/build.ninja'
    text=ninja.read_text()
    assert all(name in text for name in ('matrixgame-native-entry.elf','platform_switch.cpp','native_frame_clock.cpp','OriginalWindowsFrame.cpp'))
    capture(ninja)
    capture(ROOT/'build/matrixgame/e2e519/original-init-windows/matrixgame-native-entry-contract.vcxproj')
    protected={}
    for name,entry in previous['protected'].items():
        if entry.get('status')=='UNAVAILABLE':protected[name]=entry;continue
        assert digest(Path(name))==entry['sha256'],name
        protected[name]=entry
    hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
    for name,entry in hardware.items():
        path=ROOT/'logs/2026.10.23'/name
        assert path.stat().st_size==entry['bytes'] and digest(path)==entry['sha256'],name
    inputs=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
    assert digest(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')==inputs['package_sha256']
    for name,expected in inputs['source_cfg_sha256'].items():assert digest(ROOT/'upstream/matrixgame/MatrixGame/CFG'/name)==expected
    frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json')
    assert digest(ROOT/frozen['cache_source'])==frozen['expected_sha256']
    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    report={'date':'2026-10-25','parent':parent,'stage':'E2E-5.21 PARTIAL',
        'native_entry_prefix_host_and_arm64':'PASS with deliberate refusing capability fixture; no GPU success claimed',
        'native_Init_and_original_frame_SDK_link':'PASS','Switch_hardware_runtime':False,
        'native_full_Init_returned':False,'native_map_or_frame_executed':False,
        'Windows_regression':'two complete Init/frame/Deinit PASS; snapshots equal E2E-5.20',
        'known_next_source_boundary':'CGame::Init -> EnterPlatform -> EnterNativePlatform -> InitAsNative -> DekoDevice::ReadCaps',
        'boundary_hardware_observed':False,'GPU_alignment_hardware_verified':False,
        'original_Init_inverse_platform_translation_equal':True,'original_Draw_source_unchanged':True,
        'SDK_linked_symbols':symbols,'snapshots':snapshots,'results':results,
        'CMake_Windows_and_SDK_generated_targets_checked':True,
        'generated_sha256':generated,'source_and_artifact_sha256_at_audit':files,
        'protected':protected,'hardware_files_verified':len(hardware),'original_package_and_CFG_unchanged':True,
        'main_game_P0':'OPEN; frozen main game not modified','NRO_created':False,'push':False,'commit':False}
    (ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':report['stage'],'sdk':'Init + original frame LINK PASS','hardware':False,
        'protected_verified':sum(x.get('unchanged',False) for x in protected.values()),'hardware_files_verified':len(hardware)},ensure_ascii=False))

if __name__=='__main__':main()

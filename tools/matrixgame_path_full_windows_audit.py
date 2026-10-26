"""Collect the targeted Path/Windows results and recheck protected inputs."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e519'

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(4*1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    results={}
    for kind in ('host','arm64'):
        result=json.loads((BASE/f'path-candidate-{kind}/result.json').read_text())
        assert result['runtime_exit']==0 and not result['stderr']
        results[kind]=result
    out=BASE/'full-windows-candidate'
    results['windows_path']=json.loads((out/'path-runtime.json').read_text())
    assert results['windows_path']['runtime_exit']==0
    results['sdk']=json.loads((BASE/'path-sdk-final/result.json').read_text())
    assert all(v==0 for v in results['sdk']['results'].values()) and not results['sdk']['strong_undefined']
    results['windows_full']=json.loads((out/'result.json').read_text())
    assert all(v==0 for v in results['windows_full']['results'].values())
    results['provider']=json.loads((out/'provider-result.json').read_text())
    assert results['provider']['runtime_exit']==10 # Explicit incomplete Init, never PASS.
    stdout=results['provider']['stdout']
    assert 'HAL CreateDevice HRESULT=00000000' in stdout
    assert stdout.count('UNSUPPORTED: unowned skin provider')==2
    assert stdout.count('actual gamma readback restored; device/timer leases empty')==2
    # Resolve the actual exception stack against this exact EXE's linker map.
    mapping=(out/'windows-game.map').read_text()
    preferred=int(re.search(r'Preferred load address is ([0-9a-fA-F]+)',mapping)[1],16)
    symbols=[]
    for line in mapping.splitlines():
        m=re.match(r'\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+(\S+)\s+([0-9A-Fa-f]{8})\s',line)
        if m:symbols.append((int(m[2],16),m[1]))
    first=re.search(r'C\+\+ exception stack image=([0-9A-Fa-f]+):([^\n]+)',stdout)
    image=int(first[1],16);stack=[]
    for raw in first[2].split():
        address=int(raw,16)-image+preferred
        if not preferred<=address<preferred+0x1000000:continue
        previous=max((x for x in symbols if x[0]<=address),default=(0,'unknown'))
        stack.append({'symbol':previous[1],'offset':address-previous[0]})
    assert any('RobotPreload@CMatrixMap' in x['symbol'] for x in stack)
    assert any('LoadSpecial@CVectorObject' in x['symbol'] for x in stack)
    protected=json.loads((ROOT/'reports/matrixgame-e2e519-original-init-evidence.json').read_text())['protected']
    protected_result={}
    for path,expected in protected.items():
        if expected=='UNAVAILABLE':protected_result[path]={'status':'UNAVAILABLE'};continue
        actual=digest(Path(path));assert actual==expected,path
        protected_result[path]={'sha256':actual,'unchanged':True}
    hardware=json.loads((ROOT/'build/hardware-20261023/input-manifest.json').read_text())
    for path,entry in hardware.items():
        actual=ROOT/'logs/2026.10.23'/path
        assert actual.stat().st_size==entry['bytes'] and digest(actual)==entry['sha256'],path
    original=json.loads((BASE/'original-init-host/result.json').read_text())
    assert digest(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')==original['package_sha256']
    for name,expected in original['source_cfg_sha256'].items():
        assert digest(ROOT/'upstream/matrixgame/MatrixGame/CFG'/name)==expected
    source_names=['tools/matrixgame_effects_overrides.py','tools/matrixgame_full_game_overrides.py',
        'tools/matrixgame_engine_prepare.py','tools/e2e523_cache_frozen_audit.py',
        'tools/matrixgame_path_full_windows_audit.py','tests/test_matrixgame_path_effect.cpp',
        'tests/matrixgame_windows_game_main.cpp','tests/matrixgame_windows_game_init.cpp',
        'port/matrixgame/native-engine/platform_win32.cpp','port/matrixgame/native-engine/CMakeLists.txt']
    source_hashes={n:digest(ROOT/n) for n in source_names}
    artifacts={}
    for folder in ('path-candidate-host','path-candidate-arm64','path-sdk-final','full-windows-candidate'):
        for p in (BASE/folder).iterdir():
            if p.is_file() and (p.suffix in ('.o','.obj','.elf','.exe','.map') or p.name=='path-effect'):
                artifacts[str(p.relative_to(ROOT))]=digest(p)
    # Record reused static library inputs without rebuilding their large test suites.
    libraries=set()
    for kind in ('host','arm64'):
        j=results[kind]
        for cmd in j['commands']:
            for value in cmd:
                if value.endswith('.a'):
                    p=Path(value.replace('/mnt/d/','D:/'))
                    if not p.is_absolute():p=BASE/f'vector-group-fresh-{kind}'/p
                    libraries.add(p)
    for p in libraries:artifacts[str(p.relative_to(ROOT))]=digest(p)
    for name in re.findall(r'"([^"\n]+\.lib)"',results['windows_full']['commands']['link']):
        p=Path(name)
        if not p.is_absolute():p=BASE/'original-init-windows'/p
        if p.is_file() and p.is_relative_to(ROOT):artifacts[str(p.relative_to(ROOT))]=digest(p)
    sdk_link=next(c for c in results['sdk']['commands'] if isinstance(c,str) and 'path-effect.elf' in c)
    for name in re.findall(r'(?:[A-Za-z]:/[^\s"]+|[A-Za-z0-9_./-]+)\.a',sdk_link):
        p=Path(name)
        if not p.is_absolute():p=BASE/'text-fixed-sdk'/p
        if p.is_file() and p.is_relative_to(ROOT):artifacts[str(p.relative_to(ROOT))]=digest(p)
    git=r'C:\Program Files\Git\cmd\git.exe'
    parent=subprocess.check_output([git,'rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    baseline=BASE/'path-baseline-host/runtime.log'
    report={'date':'2026-10-25','parent':parent,'stage':'PARTIAL','path_candidate':'PASS',
        'path_scenarios':['nullptr endpoints','empty and populated lists','forced relocation',
                          'allocation refusal for both lists','retry','Release/reentry'],
        'baseline_UAF_previously_reproduced':'heap-use-after-free' in baseline.read_text(),
        'results':results,'resolved_exception_stack':stack,'source_sha256':source_hashes,
        'artifact_sha256':artifacts,'protected':protected_result,'hardware_files_verified':len(hardware),
        'original_package_and_CFG_unchanged':True,'map_load_executed':False,'run_game_loop_executed':False,
        'hardware_runtime':False,'push':False,'commit':False,
        'first_blocker':'CGame::Init -> CMatrixMap::RobotPreload -> CVectorObject::LoadSpecial: unowned CSkinManager::GetSkin provider'}
    target=ROOT/'reports/matrixgame-e2e519-path-full-windows-evidence.json'
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'path':'PASS','full_windows_link':'PASS','HAL_device_created':True,'map_load':False,
        'protected_verified':sum(v.get('unchanged',False) for v in protected_result.values()),
        'hardware_files_verified':len(hardware),'first_blocker':report['first_blocker']},ensure_ascii=False))

if __name__=='__main__':main()

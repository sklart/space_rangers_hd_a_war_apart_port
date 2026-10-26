"""Strict output/cache/Init evidence and protected input audit. No hardware inference."""
import json,re,subprocess,struct,zlib
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e527-output'
def main():
    results={}
    for kind in ('host','arm64','windows','sdk','windows-frame','entry-host','entry-arm64'):
        j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
        for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
        results[kind]=j
    payloads=[]
    for kind in ('host','arm64','windows'):
        j=results[kind];assert 'HOST_FILESYSTEM_ONLY NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN' in j['stdout'];assert len(j['output_sha256'])==1
        for n,h in j['output_sha256'].items():assert sha(ROOT/n)==h;payloads.append(h)
    assert len(set(payloads))==1
    assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
    for kind in ('entry-host','entry-arm64'):
        j=results[kind];assert 'two original cleanup cycles/output unbind PASS' in j['stdout'] and not j['full_native_Init']
    assert all('-UNDEBUG' in c['command'] for c in results['entry-host']['commands'] if c['label'].startswith('compile-'))
    sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
    for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
    ref=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];old=read(ROOT/'build/matrixgame/e2e526-render-textures/sdk/source/native-engine-source.json')['files_sha256'];assert len(ref)==252
    changed=[n for n,h in ref.items() if old[n]!=h];assert set(changed)=={'MatrixGame/src/MatrixGamePathUtils.cpp','MatrixGame/src/MatrixMinimap.cpp'}
    for directory in ('source','sdk/source'):
        assert read(BASE/directory/'native-engine-source.json')['files_sha256']==ref
        for n,h in ref.items():assert sha(BASE/directory/n)==h,n
    windows=results['windows-frame'];text=windows['stdout'];assert text.count('original CGame::Init returned: ready=1')==2 and text.count('actual SDK draws=1008')==2
    for marker in ('MAPCACHE_COLD_RENDER','MAPCACHE_STORED','MAPCACHE_LOADED'):assert text.count(marker)==1,marker
    prior=read(ROOT/'build/matrixgame/e2e525-surfaces/windows-frame/result.json');assert windows['snapshots']==prior['snapshots']
    for n,h in windows['snapshots'].items():assert sha(BASE/'windows-frame'/n)==h
    assert len(windows['output_files_sha256'])==1
    for n,h in windows['output_files_sha256'].items():
        p=BASE/'windows-frame'/n;assert sha(p)==h;data=p.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack_from('>II',data,16)==(512,512)
        offset=8;chunks=[]
        while offset<len(data):
            length=struct.unpack_from('>I',data,offset)[0];kind=data[offset+4:offset+8];chunk=data[offset+8:offset+8+length];assert zlib.crc32(kind+chunk)==struct.unpack_from('>I',data,offset+8+length)[0];offset+=12+length
            if kind==b'IDAT':chunks.append(chunk)
            if kind==b'IEND':break
        assert offset==len(data);raw=zlib.decompress(b''.join(chunks));assert len(raw)>512*512*3 and any(raw)
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
    for n in ('tools/matrixgame_original_game_overrides.py',
        'tools/matrixgame_skin_manager_overrides.py','tools/matrixgame_render_pipeline_overrides.py'):
        assert sha(ROOT/n)==original['source_and_artifact_sha256_at_audit'][n],n

    parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
    git=r'C:\Program Files\Git\cmd\git.exe';upstream=ROOT/'upstream/matrixgame'
    upstream_head=subprocess.check_output([git,'rev-parse','HEAD'],cwd=upstream,text=True).strip()
    upstream_status=subprocess.check_output([git,'status','--porcelain','--untracked-files=all'],cwd=upstream,text=True)
    assert upstream_head=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not upstream_status



    names=sorted(set(n for j in results.values() for n in j['source_sha256']))
    names+=['tools/matrixgame_output_audit.py','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e527-output.md']
    for n in names:
        s=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',line) or line.rstrip()!=line for line in s.splitlines()),n
    p=subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
    assert not list(BASE.rglob('*.nro'))
    result={'stage':'E2E-5.27 OUTPUT/CACHE PROVIDER SOFTWARE PASS / FULL NATIVE INIT PARTIAL','parent':parent,'upstream_head':upstream_head,
        'results':results,'source_sha256':{n:sha(ROOT/n) for n in names},'generated_changed':changed,
        'protected':protected,'protected_verified':checked,'hardware_inputs_verified':len(hardware),'primary_NRO':primary,
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
        'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
        'removed_guard':'CMinimap::RenderBackground -> PathToOutputFiles -> output_paths::PersonalLocation',
        'native_entry_runtime_proof':'Original Init prefix only, deliberately refusing ReadCaps fixture; two cleanup cycles/output unbind',
        'next_work':'Original minimap exception-safe target/depth/viewport/scene/resource cleanup and native cold/warm rendering; then stored-state/reset',
        'known_remaining_exception':'map stored device state backend unavailable','known_remaining_caller':'CMatrixMap::ReleasePoolDefaultResources -> CDeviceState(map_device::StateBackend())',
        'first_failing_operation_of_full_native_Init':'NOT OBSERVED; no Switch execution permitted',
        'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e527-output-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'strong_undefined':len(sdk['strong_undefined']),'remaining':result['known_remaining_exception']}))
if __name__=='__main__':main()

"""Audit only executed E2E-5.22 evidence and preserve hardware boundaries."""
import datetime,hashlib,json,re,subprocess
from pathlib import Path
from matrixgame_residency_audit import read,sha
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e522'
def main():
    results={}
    profiles=('host','arm64','perf','windows','sdk','queries-host','queries-arm64','owner-host','owner-arm64','unions-host','unions-arm64','compat','windows-frame','windows-baseline')
    for kind in profiles:
        j=read(BASE/kind/'result.json');assert j['status']=='PASS',kind
        assert all(c['exit']==0 for c in j['commands']),kind
        for n,h in j.get('source_sha256',{}).items():assert sha(ROOT/n)==h,(kind,n)
        if 'runtime_exit' in j:assert j['runtime_exit']==0,kind
        results[kind]=j
    for kind in ('host','arm64','perf'):
        s=results[kind]['stdout'];assert 'BC1 CPU PASS cases=10' in s and 'ordinary_new_OOM_positions=21 cleanup=PASS' in s
        assert 'GPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN' in s
    windows=results['windows']['stdout'];lines=[s for s in windows.splitlines() if s.startswith('case=')];assert len(lines)==57
    assert all('native_vs_D3DX_RMSE=0.00000 max=0 alpha_mismatch=0 software_vs_D3DX_max=0' in s for s in lines)
    assert 'native plan exact RGB PASS cases=3' in windows
    for kind in ('unions-host','unions-arm64'):
        s=results[kind]['stdout'];assert s.count('unions=6 composed_DDS=WINDOWS_EXACT BC1=EXACT')==2
        assert s.count('unique_decodes=127 source_reads=104 cached_bitmap_bytes=1458176 progress_calls=1536')==2
        assert 'third-upload OOM/record bounds PASS' in s and 'NO_GAME_INIT SWITCH_HARDWARE_NOT_RUN' in s
        assert 'empty/truncated/short-valid/corrupt-dimension PNG rejection and batch cleanup PASS cases=4' in s
    for kind in ('windows-frame','windows-baseline'):
        s=results[kind]['stdout'];assert s.count('original CGame::Init returned: ready=1')==2 and s.count('actual SDK draws=1008')==2
        for n,h in results[kind]['snapshots'].items():assert sha(BASE/kind/n)==h
    assert results['windows-frame']['union_sha256']==results['windows-baseline']['union_sha256']
    for kind in ('windows-frame','windows-baseline'):
        for n,h in results[kind]['union_sha256'].items():assert sha(BASE/kind/n)==h
    codec_bytes={}
    for i in range(10):
        name=('union-'+str(i)) if i<6 else ('synthetic-'+str(i-6));files=[BASE/k/(name+'-native.dds') for k in ('host','arm64','perf')]
        hashes=[sha(p) for p in files];assert len(set(hashes))==1,name
        reference=BASE/'host'/(name+'-reference.dds');assert reference.is_file()
        codec_bytes[name]={'native_sha256':hashes[0],'D3DX_payload_byte_equal':files[0].read_bytes()[128:]==reference.read_bytes()[128:]}
    sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(ROOT/sdk['elf_path'])==sdk['elf_sha256']
    for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
    builds={k:read(BASE/('clean-'+k+'-release')/'result.json') for k in ('host','sdk','tests')}
    assert all(j['status']=='PASS' and all(c['exit']==0 for c in j['commands']) for j in builds.values())
    ctest=(BASE/'clean-tests-release/ctest.log').read_text(encoding='utf-8');assert '100% tests passed, 0 tests failed out of 4' in ctest
    assert '-UNDEBUG' in (BASE/'clean-tests-release/CMakeCache.txt').read_text(encoding='utf-8')
    flags=(BASE/'clean-tests-release/core/CMakeFiles/matrixgame-native-core.dir/flags.make').read_text(encoding='utf-8');assert '-fsanitize=address,undefined' in flags and '-UNDEBUG' in flags
    repro=read(BASE/'repro-result.json');assert repro['status']=='PASS' and repro['files']==252 and repro['same_source_mtimes_preserved']
    for n,h in repro['source_sha256'].items():assert sha(ROOT/n)==h,n
    generated={}
    for directory in ('repro-source','repro-independent','clean-sdk-release/source','clean-tests-release/source'):
        marker=read(BASE/directory/'native-engine-source.json');assert marker['files_sha256']==repro['files_sha256'],directory
        for n,h in marker['files_sha256'].items():assert sha(BASE/directory/n)==h,(directory,n)
        generated[directory]=sha(BASE/directory/'OriginalTextureUnions.cpp') if (BASE/directory/'OriginalTextureUnions.cpp').is_file() else sha(BASE/directory/'MatrixGame/src/OriginalTextureUnions.cpp')
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
    names=['port/matrixgame/native-core/'+n for n in ('texture_asset.hpp','texture_mips.cpp')]
    names+=['port/matrixgame/deko/'+n for n in ('fixed_vertex.hpp','fixed_vertex.cpp','texture_store.cpp')]
    names+=['port/matrixgame/native-engine/'+n for n in ('native_texture.cpp','native_texture.hpp','bitmap_array_owner.hpp','original_texture_unions.hpp','native_map_queries.hpp','native_frame_scope.hpp','platform_switch.cpp','platform_switch.hpp','offscreen_backend.cpp','map_device_backend.cpp','transition_backend.cpp','CMakeLists.txt')]
    names+=['tools/matrixgame_map_prepare_overrides.py','tools/matrixgame_engine_prepare.py','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e522-integration.md']
    names += [str(p.relative_to(ROOT)) for p in (ROOT/'tools').glob('matrixgame_e2e522_*.py')]
    names += ['tests/'+n for n in ('test_matrixgame_bc1.cpp','matrixgame_bc1_windows.cpp','matrixgame_union_observer.cpp','matrixgame_bc1_original_owner.cpp','test_matrixgame_bc1_owner_main.cpp','matrixgame_allocation_fault.cpp','test_matrixgame_native_map_queries.cpp','test_matrixgame_original_texture_unions.cpp','probe_matrixgame_native_entry.cpp')]
    for n in names:
        text=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',s) or s.rstrip(' \t')!=s for s in text.splitlines()),n
    assert not list(BASE.rglob('*.nro'))
    command=[r'C:\Program Files\Git\cmd\git.exe','diff','--check','--',*names];p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr;(BASE/'diff-check.log').write_text(p.stdout+p.stderr)
    source=(BASE/'clean-sdk-release/source/MatrixGame/src/MatrixGame.cpp').read_text(encoding='utf-8');assert 'params = p::ReadyDevice().ReadCreationParameters();' in source
    header=(ROOT/'port/matrixgame/native-engine/platform_switch.hpp').read_text();assert 'ReadCreationParameters' not in header
    performance={}
    for kind in ('perf','arm64','unions-host','unions-arm64'):
        log=(BASE/kind/'runtime.log').read_text(encoding='utf-8');rss=re.search(r'Maximum resident set size .kbytes.: (\d+)',log);performance[kind]={'peak_process_RSS_KiB':int(rss.group(1)) if rss else None}
        rows=[]
        for line in log.splitlines():
            m=re.match(r'case=union-(\d+) encode_ms=([\d.]+) box_mips_ms=([\d.]+)',line)
            if m:rows.append({'union':int(m[1]),'encode_ms':float(m[2]),'mips_ms':float(m[3])})
        if rows:performance[kind]['six_unions_encode_plus_mips_ms']=sum(r['encode_ms']+r['mips_ms'] for r in rows);performance[kind]['rows']=rows
        performance[kind]['GPU_execution']=False
    finished=datetime.datetime.now(datetime.timezone.utc);started=datetime.datetime.fromisoformat('2026-10-25T16:50:32+00:00')
    result={'date':'2026-10-25','stage':'E2E-5.22 SOFTWARE_RESOURCE_PASS / FULL_NATIVE_INIT_PARTIAL','parent':parent,
        'work_session':{'started_UTC':started.isoformat(),'audit_finished_UTC':finished.isoformat(),
            'elapsed_seconds':(finished-started).total_seconds(),'measurement':'elapsed active session including builds/tests; not CPU time; no artificial budget wait'},
        'BC1':'actual pinned DirectX BC encoder, both opacity modes, exact six compressed-previous-level BOX mips',
        'original_compositor':'actual shared original BuildTexUnions executed from real CStorage/robots.pkg on HOST and ARM64; no fake CMatrixMap',
        'D3DX':'57 mip comparisons and 3 Clear viewport cases exact; independent old original compositor has same six DDS hashes',
        'native_frame':'acquired presentation target Clear/Scene/Present route, single original Present, real queue completion/cancel rollback wired',
        'native_frame_limit':'offscreen surfaces/targets and complete presentation reset not implemented by these acquired-frame paths',
        'Switch_runtime':False,'native_CGame_Init_executed':False,'native_PrepareMap_returned':False,'native_first_frame_presented':False,
        'next_known_direct_source_guard':'CGame::Init -> CGame::ApplyVideoParams -> Device::ReadCreationParameters',
        'next_exception':'backend creation parameters unsupported','guard_hardware_observed':False,
        'results':results,'builds':builds,'repro':repro,'generated':generated,'codec_bytes':codec_bytes,'performance':performance,
        'source_sha256':{n:sha(ROOT/n) for n in names},'protected':protected,'protected_files_verified':checked,'hardware_files_verified':len(hardware),
        'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],'primary_NRO_verified_against_previous_fingerprint':primary,
        'root_cumulative_NRO_verified_against_documented_hash':previous['root_cumulative_NRO_verified_against_documented_hash'],
        'original_package_CFG_frozen_cache_unchanged':True,'upstream_worktree_unchanged':True,'upstream_head':upstream_head,
        'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
    (ROOT/'reports/matrixgame-e2e522-integration-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'hardware_inputs':len(hardware),'next':result['next_known_direct_source_guard']}))
if __name__=='__main__':main()

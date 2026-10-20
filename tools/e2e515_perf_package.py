"""Stage a verified standalone performance NRO/ELF without modifying SD data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile
from ci_stage_switch_artifacts import matching_build_id
from e2e_arm64_build import dependency_paths

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    out=args.output.resolve();archive=out.with_suffix('.zip')
    if out.exists() or archive.exists():parser.error('Use a fresh output; immutable bundles are never replaced')
    build=ROOT/'port/switch/build/e2e-game-perf515'
    r=json.loads((build/'build-report.json').read_text())
    assert r['performance_diagnostics'] and r['build_profile']=='release'
    assert r['hardware_save_writes'] and r['experimental_audio_on'] and not r['e2e3_trace']
    assert r['arm64_link']=='PASS' and r['undefined_symbols']==0 and r['compiled']==298 and r['first_failure'] is None
    git=Path('C:/Program Files/Git/cmd/git.exe')
    if not git.is_file():git=Path('git')
    changed=subprocess.check_output([str(git),'diff','--name-only',r['build_git'],'--',
        'port/switch/platform','tools/e2e_arm64_build.py','tools/e2e_source_overrides.py','tools/e2e515_perf_overrides.py'],cwd=ROOT,text=True)
    assert not changed,'Compiled-source tree changed after build: '+changed
    sdk=Path(os.getenv('DEVKITPRO','C:/devkitPro'))
    if not sdk.is_dir():sdk=Path('C:/devkitPro')
    suffix='.exe' if os.name=='nt' else ''
    original=ROOT/r['nro']['path'];original_elf=ROOT/r['matching_elf']['path']
    assert sha(original)==r['nro']['sha256'].lower() and sha(original_elf)==r['matching_elf']['sha256'].lower()
    assert matching_build_id(original.read_bytes(),original_elf.read_bytes())==r['build_id']
    nm=subprocess.check_output([str(sdk/('devkitA64/bin/aarch64-none-elf-nm'+suffix)),'-C',str(original_elf)],text=True)
    # CpuMicros is inlined into Scope in Release; inspect the retained scopes
    # and their exact compiled-source hashes rather than requiring that symbol.
    for name in ['perf515::Initialize','perf515::Frame','perf515::TickBatch','perf515::Scope','perf515::Record','TfAB::TimerTakt','swkbdShow']:
        assert name in nm,name
    for name in ['DekoBufferRenderer','DekoMeshRenderer','pcvSetClockRate','clkrstSetClockRate','sysclkIpcSet']:
        assert name not in nm,name
    links=(build/'link-objects.rsp').read_text().replace('\\','/').lower()
    assert '/port/matrixgame/' not in links
    install=out/'switch/space-rangers-hd-a-war-apart';install.mkdir(parents=True)
    debug=out/'debug';debug.mkdir()
    elf=debug/'SpaceRangersHD-perf515.elf';shutil.copyfile(original_elf,elf)
    nacp=out/'diagnostics.nacp';nro=install/'SpaceRangersHD-perf515.nro'
    subprocess.run([str(sdk/('tools/bin/nacptool'+suffix)),'--create','Space Rangers HD E2E515 diagnostics','sklart','0.0.1',str(nacp)],check=True)
    subprocess.run([str(sdk/('tools/bin/elf2nro'+suffix)),str(elf),str(nro),'--nacp='+str(nacp),'--icon='+str(ROOT/'port/switch/assets/icon.jpg')],check=True)
    # Only hbmenu title/assets changed: every loaded NRO segment remains exact.
    old,new=original.read_bytes(),nro.read_bytes()
    for offset in (0x20,0x28,0x30):
        a,n=struct.unpack_from('<II',old,offset);b,m=struct.unpack_from('<II',new,offset)
        assert (a,n)==(b,m) and old[a:a+n]==new[b:b+m]
    assert matching_build_id(new,elf.read_bytes())==r['build_id']
    assert ('build_git='+r['build_git'][:7]).encode() in new
    assert b'perf515.csv' in new and b'perf515-stage.log' in new
    (out/'analysis').mkdir()
    shutil.copyfile(ROOT/'tools/e2e515_perf_analyze.py',out/'analysis/e2e515_perf_analyze.py')
    shutil.copyfile(ROOT/'docs/e2e515-performance-diagnostics.md',out/'README-RU.md')
    for mode in ['basic','detail','off']:
        d=out/'mode-examples'/mode;d.mkdir(parents=True)
        (d/'perf-mode.txt').write_text(mode+'\n',encoding='ascii')
    shutil.copyfile(build/'build-report.json',out/'build-report.json')
    shutil.copyfile(build/'effective-source-map.json',out/'effective-source-map.json')
    shutil.copyfile(build/'effective-import-manifest.json',out/'effective-import-manifest.json')
    inputs=set()
    for path in [*(build/'upstream').glob('*.d'),*(build/'platform').glob('*.d')]:
        dependencies=dependency_paths(path);assert dependencies,path
        inputs.update(dependencies)
    local={p.relative_to(ROOT).as_posix():sha(p) for p in inputs if p.is_relative_to(ROOT) and p.is_file()}
    (out/'source-sha256.json').write_text(json.dumps(local,indent=2)+'\n')
    summary=dict(source_commit=r['build_git'],packaging_commit=subprocess.check_output([str(git),'rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        build_id=r['build_id'],build_dirty=r['build_dirty'],hardware='PENDING; no Switch run received',
        profile='Release Save ON Audio ON; performance diagnostics; E2E3/PPM OFF',
        game_units=298,link_objects=r['link_objects'],source_inputs_hashed=len(local),
        packaging='hbmenu diagnostic title changed; loaded text/ro/data segments verified byte-identical to built NRO',
        artifacts={p.relative_to(out).as_posix():dict(bytes=p.stat().st_size,sha256=sha(p)) for p in [nro,elf]},
        original_build_nro=r['nro'],analyzer_sha256=sha(out/'analysis/e2e515_perf_analyze.py'))
    (out/'manifest.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (out/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.relative_to(out).as_posix()+'\n' for p in sorted(out.rglob('*')) if p.is_file()))
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(out.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(out).as_posix())
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for line in z.read('SHA256SUMS').decode().splitlines():
            digest,path=line.split('  ',1);assert hashlib.sha256(z.read(path)).hexdigest()==digest,path
    summary['bundle']=dict(path=str(archive),bytes=archive.stat().st_size,sha256=sha(archive))
    (out.parent/(out.name+'-result.json')).write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary,ensure_ascii=False,indent=2))
    return 0
if __name__=='__main__':raise SystemExit(main())

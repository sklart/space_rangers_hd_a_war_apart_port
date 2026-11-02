"""Audit corrected diagnostic handoff, SDK smoke and preserved first delivery."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e546-window-handoff'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
 profiles={}
 for kind in ('host','arm64'):
  j=read(BASE/kind/'result.json');profiles[kind]=j;assert j['status']=='PASS'
  assert all(c['exit']==0 for c in j['commands'])
  for path,digest in j['source_sha256'].items():assert sha(ROOT/path)==digest,path
  assert sha(BASE/kind/'handoff-test')==j['binary_sha256']
  assert 'baseline_not_attached reproduced original_InitAsNative executed exclusive_peak=1' in j['stdout']
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 assert 'qemu-aarch64' in profiles['arm64']['commands'][1]['command']
 sdk=read(BASE/'sdk/result.json');profiles['sdk']=sdk
 assert sdk['status']=='COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' and not sdk['strong_undefined']
 assert all(c['exit']==0 for c in sdk['commands'])
 for path,digest in sdk['source_sha256'].items():assert sha(ROOT/path)==digest,path
 assert sha(ROOT/sdk['elf_path'])==sdk['elf_sha256']
 for path,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/path)==digest,path
 metadata=read(BASE/'sdk/native-diagnostic-build.json')
 expected={str(p.relative_to(ROOT)).replace('\\','/') for p in (ROOT/'port/matrixgame').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.h','.inc','.inl','.cmake','.glsl','.txt')}
 expected.update(str(p.relative_to(ROOT)).replace('\\','/') for p in (ROOT/'tools').glob('matrixgame_*.py'))
 expected.update(['tests/probe_matrixgame_native_entry.cpp','tests/matrixgame_native_diagnostic_main.cpp'])
 assert set(metadata['own_source_sha256'])==expected
 for path,digest in metadata['own_source_sha256'].items():assert sha(ROOT/path)==digest,path
 original={k:v for k,v in metadata.items() if k not in ('source_sha256','build_id')}
 assert hashlib.sha256(json.dumps(original,sort_keys=True,separators=(',',':')).encode()).hexdigest()==metadata['source_sha256']
 assert metadata['deko_library']=='deko3dd'
 assert sha(Path('C:/devkitPro/libnx/lib/libdeko3dd.a'))==metadata['deko_archive_sha256']
 assert sha(Path('C:/devkitPro/libnx/lib/libnx.a'))==metadata['libnx_archive_sha256']
 first=read(ROOT/'dist/matrixgame-e2e527-diag/build-inputs.json')
 assert metadata['generated_original_sha256']==first['generated_original_sha256'],'original Init/Draw generation changed'
 for path,digest in metadata['generated_original_sha256'].items():assert sha(BASE/'sdk/source'/path)==digest,path
 # Reuse unchanged CPU components, do not rerun the historical broad suites.
 reused={}
 for name in ('host','arm64','journal/host','journal/arm64'):
  path=ROOT/'build/matrixgame/e2e544-smoke'/name/'result.json';j=read(path);assert j['status']=='PASS'
  for source,digest in j['source_sha256'].items():assert sha(ROOT/source)==digest,source
  reused[name]=dict(result_sha256=sha(path),scope='UNCHANGED CPU CHECKER/JOURNAL ONLY')
 # Verify the entire first package, including its original readme/manifest.
 firstdir=ROOT/'dist/matrixgame-e2e527-diag';first_files={}
 for line in (firstdir/'SHA256SUMS.txt').read_text().splitlines():
  digest,name=line.split('  ',1);assert sha(firstdir/name)==digest,name;first_files[name]=digest
 assert sha(firstdir/'matrixgame-e2e527-diag.nro')=='54fa67f6351eaadc52cfc838bd9b501929116346a92e6759c3ae92972fd68867'
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');protected=0
 for path,record in previous['protected'].items():
  if record.get('status')!='UNAVAILABLE':assert sha(Path(path))==record['sha256'],path;protected+=1
 for path,record in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(path))==record['sha256'],path
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for path,record in hardware.items():assert sha(ROOT/'logs/2026.10.23'/path)==record['sha256'],path
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 git=r'C:\Program Files\Git\cmd\git.exe'
 def gitout(*args):return subprocess.check_output([git,*args],cwd=ROOT/'upstream/matrixgame',text=True).strip()
 assert gitout('rev-parse','HEAD')=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not gitout('status','--porcelain','--untracked-files=all')
 result=dict(status='E546 WINDOW HANDOFF SOFTWARE PASS; SECOND NRO PACKAGING SEPARATE',profiles=profiles,
  reused_unchanged_tests=reused,metadata=metadata,first_package_sha256=first_files,
  protected_verified=protected,hardware_inputs_verified=len(hardware),
  candidate_Switch_hardware='NOT RUN / HARDWARE_PENDING',candidate_smoke_rendered=False,
  original_InitAsNative_CPU_executed=True,full_CGame_Init_candidate_executed=False,commit=False,push=False,
  source_sha256={path:sha(ROOT/path) for path in ('tools/matrixgame_handoff_audit.py','docs/matrixgame-e2e527-second-run.md')})
 (ROOT/'reports/matrixgame-e2e546-window-handoff-evidence.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','protected_verified','hardware_inputs_verified')}))
if __name__=='__main__':main()

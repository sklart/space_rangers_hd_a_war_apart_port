"""Audit actual durable journal, original Windows stages and diagnostic SDK ELF."""
import hashlib,json,re,subprocess
from pathlib import Path
from matrixgame_journal_checks import BASE
from matrixgame_draw_budget_checks import ROOT,sha
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
 profiles={k:read(BASE/k/'result.json') for k in ('host','arm64','sdk')};profiles['windows']=read(BASE/'map-state/windows/result.json')
 for kind,j in profiles.items():
  assert j['status']==('COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if kind=='sdk' else 'PASS'),(kind,j['status'])
  assert all(c['exit']==0 for c in j['commands']),kind
  for name,digest in j['source_sha256'].items():assert sha(ROOT/name)==digest,(kind,name)
 for kind in ('host','arm64'):
  j=profiles[kind];assert 'fault_cases=7' in j['stdout'] and 'concurrent_records=200' in j['stdout']
  for name,digest in j['verified_journals_sha256'].items():assert sha(ROOT/name)==digest
  assert sha(BASE/kind/'journal-test')==j['binary_sha256']
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 windows=profiles['windows'];assert windows['stdout'].count('REAL_RESOURCE_EARLY_FAILURE')==2 and windows['stdout'].count('REAL_RESOURCE_SCOPE')==2
 driver=(BASE/'map-state/windows/driver.cpp').read_text(encoding='utf-8')
 log_root=Path(re.search(r'#define MATRIXGAME_FORM_TEST_OUTPUT L"([^"]+)"',driver)[1])/'diagnostic-logs'
 logs=sorted(log_root.glob('*/run.jsonl'));assert len(logs)==2
 required={'B.CFile::OpenPackFiles','B.config.text','B.CacheInit','B.platform_attachment','B.debug_font.CreateFor','B.cursor.constructor','B.map.constructor','B.RobotPreload','B.stor.Load','B.BuildTexUnions','B.PrepareMap','B.ApplyVideoParams','B.CMinimap::RenderBackground','B.interface_and_final_resources','B.CGame::Init'}
 for path in logs:
  rows=[json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
  assert rows[0]['event']=='journal_open' and rows[-1]['event']=='journal_close'
  assert [r['seq'] for r in rows]==list(range(1,len(rows)+1))
  assert {r['operation'] for r in rows if r['event']=='ok'}==required
  failures=[r for r in rows if r['event']=='failed'];assert [r['operation'] for r in failures]==['B.CFile::OpenPackFiles','B.CGame::Init']
  assert all(r['result'] is None for r in failures)
  stack=[]
  for row in rows:
   if row['event']=='begin':stack.append(row['operation'])
   if row['event'] in ('ok','failed'):assert stack.pop()==row['operation']
  assert not stack
 sdk=profiles['sdk'];assert not sdk['strong_undefined'] and len(sdk['link_inputs_sha256'])==185
 assert sdk['original_generated_files_unchanged']==249 and len(sdk['original_generated_files_changed'])==6
 assert sha(ROOT/sdk['elf_path'])==sdk['elf_sha256']
 for name,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/name)==digest,name
 metadata=read(BASE/'sdk/native-diagnostic-build.json')
 for name,digest in metadata['own_source_sha256'].items():assert sha(ROOT/name)==digest,('metadata',name)
 original={k:v for k,v in metadata.items() if k not in ('source_sha256','build_id')}
 assert hashlib.sha256(json.dumps(original,sort_keys=True,separators=(',',':')).encode()).hexdigest()==metadata['source_sha256']
 assert sha(Path('C:/devkitPro/libnx/lib/libnx.a'))==metadata['libnx_archive_sha256']
 assert sha(Path('C:/devkitPro/libnx/lib/libdeko3d.a'))==metadata['deko_archive_sha256']
 assert 'fsFileFlush' in (BASE/'sdk-reference/fsdev_fsync.txt').read_text()
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');protected=0
 for name,value in previous['protected'].items():
  if value.get('status')!='UNAVAILABLE':assert sha(Path(name))==value['sha256'],name;protected+=1
 for name,value in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(name))==value['sha256'],name
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for name,value in hardware.items():assert sha(ROOT/'logs/2026.10.23'/name)==value['sha256'],name
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 assert output('rev-parse','HEAD',cwd=ROOT/'upstream/matrixgame')=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all',cwd=ROOT/'upstream/matrixgame')
 assert not list(BASE.rglob('*.nro'))
 names={p for j in profiles.values() for p in j['source_sha256']};names|={'tools/matrixgame_journal_audit.py','tools/matrixgame_actual_draw_checks.py','reports/matrixgame-e2e543-journal.md','docs/matrixgame-init-sequence.md','docs/matrixgame-readiness-plan.md'}
 result=dict(status='E2E-5.43 JOURNAL HOST/ARM64/WINDOWS/SDK PASS; DIAGNOSTIC NRO PREPARATION PARTIAL',profiles=profiles,
  source_sha256={p:sha(ROOT/p) for p in sorted(names)},Windows_journals_sha256={str(p.relative_to(ROOT)):sha(p) for p in logs},build_metadata=metadata,
  sdk_fsync_reference_sha256=sha(BASE/'sdk-reference/fsdev_fsync.txt'),parent=output('rev-parse','HEAD'),protected_verified=protected,hardware_inputs_verified=len(hardware),
  native_Init_executed=False,native_frame_presented=False,Switch_hardware='NOT RUN / HARDWARE_PENDING',diagnostic_NRO_ready=False,NRO_created=False,commit=False,push=False,
  remaining=['standalone real GPU smoke including BC1 upload/present/release','on-screen diagnostic status/error','GPU readback/checksum if supported','finish fault/cleanup and controlled-exit entry validation','NRO generation/package manifest/assets/instructions'])
 (ROOT/'reports/matrixgame-e2e543-journal-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

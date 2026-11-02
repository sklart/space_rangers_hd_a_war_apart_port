"""Audit E544 software evidence; never substitutes CPU pixels for GPU execution."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e544-smoke'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
 profiles={}
 for name in ('host','arm64','journal/host','journal/arm64','sdk'):
  result=read(BASE/name/'result.json');profiles[name]=result
  expected='COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if name=='sdk' else 'PASS'
  assert result['status']==expected,(name,result['status'])
  assert all(c['exit']==0 for c in result['commands']),name
  for path,digest in result['source_sha256'].items():assert sha(ROOT/path)==digest,(name,path)
  if name.startswith('journal/'):
   for path,digest in result['verified_journals_sha256'].items():assert sha(ROOT/path)==digest,path
   assert sha(BASE/name/'journal-test')==result['binary_sha256']
  elif name!='sdk':assert sha(BASE/name/'pixels-test')==result['binary_sha256']
 for name in ('host','journal/host'):assert '-fsanitize=address,undefined' in profiles[name]['commands'][0]['command']
 for name in ('arm64','journal/arm64'):assert 'qemu-aarch64' in profiles[name]['commands'][1]['command']
 sdk=profiles['sdk'];assert not sdk['strong_undefined']
 assert sdk['original_generated_files_unchanged']==249 and len(sdk['original_generated_files_changed'])==6
 assert sha(ROOT/sdk['elf_path'])==sdk['elf_sha256']
 for path,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/path)==digest,path
 metadata=read(BASE/'sdk/native-diagnostic-build.json')
 for path,digest in metadata['own_source_sha256'].items():assert sha(ROOT/path)==digest,path
 encoded={k:v for k,v in metadata.items() if k not in ('source_sha256','build_id')}
 assert hashlib.sha256(json.dumps(encoded,sort_keys=True,separators=(',',':')).encode()).hexdigest()==metadata['source_sha256']
 assert metadata['deko_library']=='deko3dd'
 assert sha(Path('C:/devkitPro/libnx/lib/libdeko3dd.a'))==metadata['deko_archive_sha256']
 assert sha(Path('C:/devkitPro/libnx/lib/libnx.a'))==metadata['libnx_archive_sha256']
 for path,digest in metadata['generated_original_sha256'].items():assert sha(BASE/'sdk/source'/path)==digest,path
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
 result=dict(status='E544 SOFTWARE PASS; NRO PACKAGING PENDING',profiles=profiles,metadata=metadata,
  protected_verified=protected,hardware_inputs_verified=len(hardware),reachable_link_inputs=len(sdk['link_inputs_sha256']),
  Switch_hardware='NOT RUN / HARDWARE_PENDING',GPU_smoke_executed=False,native_Init_executed=False,
  native_frame_presented=False,NRO_created=False,commit=False,push=False,
  source_sha256={p:sha(ROOT/p) for p in ('tools/matrixgame_smoke_audit.py','reports/matrixgame-e2e544-smoke.md')},
  remaining=['package one diagnostic NRO with NACP/icon and validate NRO sections/assets',
             'reproducible build command, manifest and exact resource list',
             'first hardware run instructions and log/readback extraction',
             'final requirement-by-requirement preparation audit'])
 (ROOT/'reports/matrixgame-e2e544-smoke-evidence.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','protected_verified','hardware_inputs_verified','reachable_link_inputs')}))
if __name__=='__main__':main()

"""Audit run2 hardware boundary and offline poll fix; never packages a third NRO."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e547-applet-poll'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
 profiles={}
 for kind in ('host','arm64'):
  j=read(BASE/kind/'result.json');profiles[kind]=j;assert j['status']=='PASS'
  assert all(c['exit']==0 for c in j['commands'])
  for path,digest in j['source_sha256'].items():assert sha(ROOT/path)==digest,path
  for path,digest in j['SDK_headers_sha256'].items():assert sha(Path(path))==digest,path
  assert sha(BASE/kind/'applet-poll-test')==j['binary_sha256']
  assert 'run2_59905_baseline_reproduced original_libnx_GetMessage executed' in j['stdout']
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 assert 'qemu-aarch64' in profiles['arm64']['commands'][1]['command']
 sdk=read(BASE/'sdk/result.json');profiles['sdk']=sdk
 assert sdk['status']=='COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' and not sdk['strong_undefined']
 assert all(c['exit']==0 for c in sdk['commands'])
 for path,digest in sdk['source_sha256'].items():assert sha(ROOT/path)==digest,path
 assert sha(ROOT/sdk['elf_path'])==sdk['elf_sha256']
 for path,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/path)==digest,path
 metadata=read(BASE/'sdk/native-diagnostic-build.json')
 for path,digest in metadata['own_source_sha256'].items():assert sha(ROOT/path)==digest,path
 original={k:v for k,v in metadata.items() if k not in ('source_sha256','build_id')}
 assert hashlib.sha256(json.dumps(original,sort_keys=True,separators=(',',':')).encode()).hexdigest()==metadata['source_sha256']
 assert metadata['generated_original_sha256']==read(ROOT/'dist/matrixgame-e2e527-diag-run2/build-inputs.json')['generated_original_sha256']
 for path,digest in metadata['generated_original_sha256'].items():assert sha(BASE/'sdk/source'/path)==digest,path
 assert metadata['deko_library']=='deko3dd'
 assert sha(Path('C:/devkitPro/libnx/lib/libdeko3dd.a'))==metadata['deko_archive_sha256']
 assert sha(Path('C:/devkitPro/libnx/lib/libnx.a'))==metadata['libnx_archive_sha256']
 preserved={}
 for folder,report in [('matrixgame-e2e527-diag','matrixgame-e2e527-diagnostic-delivery-evidence.json'),('matrixgame-e2e527-diag-run2','matrixgame-e2e546-run2-delivery-evidence.json')]:
  delivery=read(ROOT/'reports'/report)
  for name,digest in delivery['package_files_sha256'].items():assert sha(ROOT/'dist'/folder/name)==digest,(folder,name)
  preserved[folder]=delivery['package_files_sha256']
 hardware=read(ROOT/'reports/matrixgame-e2e547-run2-hardware-evidence.json')
 assert sha(BASE/'run2.jsonl')==hardware['journal_sha256']
 assert sha(BASE/'libnx-applet-v4.12.0.c')==hardware['libnx_source_sha256']
 assert sha(BASE/'original-applet-get-message.inc')==hardware['reference_function_sha256']
 assert hardware['result_decimal']==59905 and hardware['window_attachment_ok'] and not hardware['original_Init_reached']
 # Preserve the CPU handoff proof; no corresponding ownership source changed.
 reused={}
 for kind in ('host','arm64'):
  path=ROOT/'build/matrixgame/e2e546-window-handoff'/kind/'result.json';j=read(path)
  assert j['status']=='PASS'
  for source,digest in j['source_sha256'].items():assert sha(ROOT/source)==digest,source
  reused[kind]=sha(path)
 prior=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');protected=0
 for path,record in prior['protected'].items():
  if record.get('status')!='UNAVAILABLE':assert sha(Path(path))==record['sha256'],path;protected+=1
 for path,record in prior['extra_frozen_artifacts_verified'].items():assert sha(Path(path))==record['sha256'],path
 primary=prior['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==prior['root_cumulative_NRO_verified_against_documented_hash']
 inputs=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for path,record in inputs.items():assert sha(ROOT/'logs/2026.10.23'/path)==record['sha256'],path
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args):return subprocess.check_output([git,*args],cwd=ROOT/'upstream/matrixgame',text=True).strip()
 assert output('rev-parse','HEAD')=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all')
 assert not list(BASE.rglob('*.nro'))
 result=dict(status='E547 POLL FIX HOST/ARM64/SDK PASS; HARDWARE FIX NOT RUN',profiles=profiles,
  hardware_run2=hardware,metadata=metadata,hand_off_tests_reused_sha256=reused,
  preserved_packages=preserved,protected_verified=protected,hardware_inputs_verified=len(inputs),
  fixed_GPU_smoke_executed=False,full_native_Init_executed=False,frame_presented=False,
  third_NRO_created=False,commit=False,push=False,
  source_sha256={p:sha(ROOT/p) for p in ('tools/matrixgame_applet_poll_audit.py','reports/matrixgame-e2e547-applet-poll.md')})
 (ROOT/'reports/matrixgame-e2e547-applet-poll-evidence.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','protected_verified','hardware_inputs_verified','third_NRO_created')}))
if __name__=='__main__':main()

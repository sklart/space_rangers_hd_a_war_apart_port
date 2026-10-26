"""Verify reset ownership evidence without converting a failing HAL oracle to PASS."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e534-reset-owners'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 profiles={n:read(BASE/n/'result.json') for n in ('baseline','host','arm64','sdk','windows','regression-host')}
 for n,j in profiles.items():
  if n=='windows':
   assert j['status']=='FAIL' and j['commands'][-1]['exit']==10
   assert 'cases=15 mismatches=2' in j['stdout']
   assert 'state_block_released_before_first_reset hr=00000000' in j['stdout']
  else:assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),n
  for p,h in j['source_sha256'].items():assert sha((BASE/'baseline/source'/p) if n=='baseline' else ROOT/p)==h,(n,p)
  if 'binary_sha256' in j:
   executable={'baseline':'baseline','host':'reset-owners','arm64':'reset-owners','windows':'oracle.exe'}[n]
   assert sha(BASE/n/executable)==j['binary_sha256']
 for n in ('host','arm64'):
  assert 'production Reset ownership PASS' in profiles[n]['stdout']
  assert 'CPU_FIXTURES NO_NATIVE_INIT NO_GPU NO_SWITCH_HARDWARE' in profiles[n]['stdout']
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 assert 'BASELINE production default-resource guard blocks renderer-only texture' in profiles['baseline']['stdout']
 for n,j in profiles['regression-host']['profiles'].items():assert sha(BASE/'regression-host'/n)==j['binary_sha256']
 sdk=profiles['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for p,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/p)==h,p
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256']
 previous=read(ROOT/'build/matrixgame/e2e533-form/sdk/source/native-engine-source.json')['files_sha256']
 assert generated==previous and len(generated)==255
 for p,h in generated.items():assert sha(BASE/'sdk/source'/p)==h,p
 old=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');checked=0
 for n,v in old['protected'].items():
  if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
 for n,v in old['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
 primary=old['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==old['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 for n,h in read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 assert output('rev-parse','HEAD')==old['parent']
 up=ROOT/'upstream/matrixgame';assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all',cwd=up)
 names={p for n,j in profiles.items() if n!='baseline' for p in j['source_sha256']}
 names.update(['tools/matrixgame_reset_owners_checks.py','tools/matrixgame_reset_owners_audit.py','reports/matrixgame-e2e534-reset-owners.md','docs/matrixgame-readiness-plan.md','docs/matrixgame-native-providers.md'])
 subprocess.run([git,'diff','--check','--',*sorted(names)],cwd=ROOT,check=True)
 for n in names:
  assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.34 PARTIAL: NATIVE OWNERSHIP SOFTWARE PASS; WINDOWS HAL RETRY FAIL; SWITCH RUNTIME NOT RUN',
  'profiles':profiles,'sdk_reachable_own_link_inputs':len(sdk['link_inputs_sha256']),'original_generated_files_unchanged':len(generated),
  'source_sha256':{n:sha(ROOT/n) for n in sorted(names)},'protected_verified':checked,'hardware_inputs_verified':len(hardware),
  'native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
  'first_native_runtime_blocker':'NOT OBSERVED; native Init has not executed on Switch',
  'Windows_HAL_open_failure':'Reset retry after releasing state block: 2 INVALIDCALL vs expected S_OK; before-first-Reset release succeeds',
  'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 result['initial_HAL_failure_artifacts']={str(p.relative_to(BASE)):sha(p) for p in (BASE/'windows-initial-failure').iterdir() if p.is_file()}
 (ROOT/'reports/matrixgame-e2e534-reset-owners-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','sdk_reachable_own_link_inputs','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

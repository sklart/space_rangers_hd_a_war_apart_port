"""Addressed UI text output-contract follow-up; no native runtime/HW claim."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e529-text-contract'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 results={}
 for kind in ('host','arm64','sdk'):
  j=read(BASE/kind/'result.json')
  assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
  for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
  results[kind]=j
 for kind in ('host','arm64'):
  j=results[kind]
  assert 'glyph/layout goldens=17940;' in j['stdout']
  assert 'write-only-factory-outputs PASS' in j['stdout']
  assert 'CPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN' in j['stdout']
  assert '-UNDEBUG' in j['commands'][0]['command']
  if 'binary_sha256' in j:assert sha(BASE/kind/'native-text')==j['binary_sha256']
 assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
 initial={}
 for kind in ('host','arm64'):
  j=read(BASE/kind/'initial-test-result.json')
  assert j['status']=='INCOMPLETE' and j['commands'][-1]['exit']==6
  log=BASE/kind/'initial-test-runtime.log'
  assert 'rejected && !poisonSurface' in log.read_text(encoding='utf-8')
  initial[kind]={'result_sha256':sha(BASE/kind/'initial-test-result.json'),'runtime_log_sha256':sha(log)}
 oracle=read(ROOT/'build/matrixgame/e2e524-text/windows/result.json')
 assert oracle['status']=='PASS' and all(c['exit']==0 for c in oracle['commands'])
 assert 'pixel/layout cases=1164 exact' in oracle['stdout'] and oracle['stdout'].count('pixel/layout cases=5592 exact')==3
 for n in ('port/matrixgame/native-engine/native_text.cpp','tests/matrixgame_text_font_export.cpp'):
  assert sha(ROOT/n)==oracle['source_sha256'][n],n
 for n,h in oracle['assets_sha256'].items():assert sha(ROOT/n)==h,n
 assert sha(ROOT/'upstream/matrixgame/MatrixGame/Rangers.ttf')==oracle['rangers_ttf_sha256']
 sdk=results['sdk'];assert not sdk['strong_undefined']
 assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256']
 assert generated==read(ROOT/'build/matrixgame/e2e528-minimap/sdk/source/native-engine-source.json')['files_sha256']
 for n,h in generated.items():assert sha(BASE/'sdk/source'/n)==h,n
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');checked=0
 for n,v in previous['protected'].items():
  if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
 for n,v in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 for n,h in read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 inventory=read(ROOT/'reports/matrixgame-e2e528-stored-state-inventory.json')
 for n,h in inventory['source_sha256'].items():assert sha(ROOT/n)==h,n
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 parent=output('rev-parse','HEAD');assert parent==previous['parent']
 upstream=ROOT/'upstream/matrixgame';assert output('rev-parse','HEAD',cwd=upstream)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all',cwd=upstream)
 names=['port/matrixgame/native-engine/native_text_backend.cpp','tests/test_matrixgame_native_text.cpp','tools/matrixgame_text_checks.py','tools/matrixgame_text_followup_audit.py','reports/matrixgame-text-contract-followup.md','docs/matrixgame-readiness-plan.md']
 subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,check=True)
 assert not list(BASE.rglob('*.nro'))
 result={'status':'TEXT SOFTWARE PASS / FULL NATIVE INIT PARTIAL','results':results,
  'executables_sha256':{kind:sha(BASE/kind/'native-text') for kind in ('host','arm64')},'initial_test_diagnostics':initial,
  'source_sha256':{n:sha(ROOT/n) for n in names},'parent':parent,
  'windows_HAL':'REUSED E2E-5.24, raster/exporter/assets hashes verified; no new Windows runtime',
  'windows_full_regression':'NOT REPEATED; native-only factory outputs changed',
  'generated_original_files_unchanged':len(generated),'protected_verified':checked,'hardware_inputs_verified':len(hardware),
  'original_Rangers_ttf_sha256':oracle['rangers_ttf_sha256'],
  'remaining_exception':'map stored device state backend unavailable',
  'remaining_caller':'CMatrixMap::ReleasePoolDefaultResources -> CDeviceState(map_device::StateBackend())',
  'remaining_scope':'source-traced release/reset branch; not observed first failure of native Init',
  'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,
  'Switch_hardware':'NOT RUN / HARDWARE_PENDING','NRO_created':False,'commit':False,'push':False,
  'initial_test_error':'mip 1 exists in full chain; corrected test to UINT_MAX; initial logs retained',
  'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-text-contract-followup-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','generated_original_files_unchanged','protected_verified','hardware_inputs_verified','remaining_exception')}))
if __name__=='__main__':main()

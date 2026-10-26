"""Audit explicit resource-root evidence without running hardware or changing assets."""
import json,subprocess
from pathlib import Path
from matrixgame_resource_checks import BASE
from matrixgame_draw_budget_checks import ROOT,sha

def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
 profiles={k:read(BASE/k/'result.json') for k in ('host','arm64','host-file','arm64-file','sdk')}
 profiles['windows']=read(BASE/'map-state/windows/result.json')
 baseline=read(BASE/'baseline/result.json');assert baseline['status']=='BASELINE REPRODUCED'
 for kind,result in profiles.items():
  assert result['status']==('COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if kind=='sdk' else 'FAIL' if kind=='windows' else 'PASS'),kind
  assert all(c['exit']==0 for c in result['commands'] if kind!='windows' or c['label']!='runtime'),kind
  for name,digest in result['source_sha256'].items():assert sha(ROOT/name)==digest,(kind,name)
 for kind in ('host','arm64'):
  assert 'map_id=389853e2' in profiles[kind]['stdout']
  assert 'worker_reads=16' in profiles[kind]['stdout']
  assert 'NO_PREPAREMAP NO_GPU' in profiles[kind]['stdout']
 for kind in ('host','host-file'):assert '-fsanitize=address,undefined' in profiles[kind]['commands'][0]['command']
 windows=profiles['windows']['stdout']
 for marker in ('REAL_RESOURCE_EARLY_FAILURE package_open_rejected=PASS original_cleanup=PASS root_cleanup=PASS','REAL_RESOURCE_SCOPE full_Init_PrepareMap=PASS foreign_cwd_unchanged=PASS root_through_recovery=PASS cleanup=PASS'):
  assert windows.count(marker)==1,(marker,windows.count(marker))
 assert profiles['windows']['commands'][-1]['label']=='runtime'
 assert profiles['windows']['commands'][-1]['exit']!=0
 sdk=profiles['sdk'];assert not sdk['strong_undefined'] and len(sdk['link_inputs_sha256'])==184
 assert sdk['original_generated_files_unchanged']==255
 for name,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/name)==digest,name
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');protected=0
 for name,value in previous['protected'].items():
  if value.get('status')!='UNAVAILABLE':assert sha(Path(name))==value['sha256'],name;protected+=1
 for name,value in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(name))==value['sha256'],name
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for name,value in hardware.items():assert sha(ROOT/'logs/2026.10.23'/name)==value['sha256'],name
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 for name,digest in read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/name)==digest,name
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 assert output('rev-parse','HEAD',cwd=ROOT/'upstream/matrixgame')=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all',cwd=ROOT/'upstream/matrixgame')
 assert not list(BASE.rglob('*.nro'))
 names={p for result in profiles.values() for p in result['source_sha256']}
 names|={'tools/matrixgame_resource_audit.py','docs/matrixgame-init-sequence.md','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e541-resource-root.md'}
 result=dict(status='E2E-5.41 PARTIAL: HOST/ARM64/SDK PASS; WINDOWS REPEATED FAILURE FAIL; SWITCH HARDWARE PENDING',profiles=profiles,baseline=baseline,
  source_sha256={p:sha(ROOT/p) for p in sorted(names)},parent=output('rev-parse','HEAD'),protected_verified=protected,hardware_inputs_verified=len(hardware),
  native_Init_executed=False,native_map_loaded=False,native_frame_presented=False,Switch_hardware='NOT RUN / HARDWARE_PENDING',NRO_created=False,
  first_native_runtime_blocker='NOT OBSERVED ON SWITCH',Windows_blocker='heap corruption on second missing-package Init after successful first Init/PrepareMap/Draw/Deinit; cause unresolved',P0='OPEN',GI_repeating_text_causality='NOT PROVEN',manual_publish_ENOENT='OPEN',Switch_hangs='OPEN',E2E534_HAL_retry='OPEN / FAIL',queue_loss_recovery='OPEN')
 (ROOT/'reports/matrixgame-e2e541-resource-root-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

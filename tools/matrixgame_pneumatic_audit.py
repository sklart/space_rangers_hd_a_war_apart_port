"""Verify pneumatic baseline/candidate and genuine repeated original Windows Init."""
import json,subprocess
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
from matrixgame_pneumatic_checks import BASE
from matrixgame_vector_object_overrides import function
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def main():
 profiles={k:read(BASE/k/'result.json') for k in ('baseline','host','arm64','sdk')}
 profiles['windows']=read(BASE/'map-state/windows/result.json')
 for kind,j in profiles.items():
  expected='BASELINE REPRODUCED' if kind=='baseline' else 'COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if kind=='sdk' else 'PASS'
  assert j['status']==expected,(kind,j['status'])
  for cmd in j['commands']:assert cmd['exit']==0 or (kind=='baseline' and cmd['label']=='runtime' and cmd['exit']!=0)
  for name,digest in j['source_sha256'].items():assert sha(ROOT/name)==digest,(kind,name)
 assert 'double-free' in profiles['baseline']['stdout']
 for kind in ('baseline','host','arm64'):
  j=profiles[kind];assert sha(ROOT/j['method_source'])==j['method_source_sha256']
  assert function((ROOT/j['method_source']).read_text(encoding='utf-8'),'void CMatrixRobot::DestroyPneumaticData(void)')==j['method']
  assert sha(BASE/kind/'pneumatic-release')==j['binary_sha256']
 for kind in ('baseline','host'):assert '-fsanitize=address,undefined' in profiles[kind]['commands'][0]['command']
 for kind in ('host','arm64'):assert 'PNEUMATIC_RELEASE PASS' in profiles[kind]['stdout']
 old=profiles['baseline']['method'];new=profiles['host']['method']
 assert new==old[:-1]+'    m_Pneumaic = nullptr;\n    g_fcnt = 0;\n}'
 windows=profiles['windows'];text=windows['stdout']
 for marker in ('REAL_RESOURCE_EARLY_FAILURE package_open_rejected=PASS original_cleanup=PASS root_cleanup=PASS','REAL_RESOURCE_SCOPE full_Init_PrepareMap=PASS foreign_cwd_unchanged=PASS root_through_recovery=PASS cleanup=PASS','REAL_FORM_LOOP draws=8 elapsed=125','ACTUAL_MAP_NATIVE_STATE'):
  assert text.count(marker)==2,(marker,text.count(marker))
 assert sha(BASE/'map-state/windows/matrixgame-map-state.exe')==windows['binary_sha256']
 for p,digest in windows['capture_sha256'].items():assert sha(ROOT/p)==digest
 sdk=profiles['sdk'];assert not sdk['strong_undefined'] and len(sdk['link_inputs_sha256'])==184
 assert sdk['original_generated_files_unchanged']==254 and sdk['original_generated_files_changed']==['MatrixGame/src/MatrixObjectRobot.cpp']
 assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for p,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/p)==digest
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
 names={p for j in profiles.values() for p in j['source_sha256']}
 names|={'tools/matrixgame_pneumatic_audit.py','tools/matrixgame_actual_draw_checks.py','docs/matrixgame-init-sequence.md','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e542-pneumatic.md'}
 result=dict(status='E2E-5.42 PNEUMATIC RELEASE / REPEATED WINDOWS INIT PASS; SWITCH HARDWARE PENDING',profiles=profiles,
  source_sha256={p:sha(ROOT/p) for p in sorted(names)},parent=output('rev-parse','HEAD'),protected_verified=protected,hardware_inputs_verified=len(hardware),
  source_method='CMatrixRobot::DestroyPneumaticData',resource_root_Windows_gate='PASS / E2E-5.41 followup',native_Init_executed=False,native_frame_presented=False,
  Switch_hardware='NOT RUN / HARDWARE_PENDING',diagnostic_NRO_ready=False,next_preparation_blocker='probe main lacks early persistent journal, bounded diagnostic stages and Base::CException boundary',NRO_created=False,commit=False,push=False)
 (ROOT/'reports/matrixgame-e2e542-pneumatic-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

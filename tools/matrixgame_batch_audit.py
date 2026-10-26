"""Strict batching evidence: real HAL demand, CPU policy, SDK, protected inputs."""
import csv,json,subprocess
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,BASE,sha
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def main():
 profiles={name:read(BASE/name/'result.json') for name in ('baseline-windows','host','arm64','sdk')}
 baseline=profiles['baseline-windows'];assert baseline['status']=='BASELINE SOURCE_POLICY_REJECT'
 assert baseline['commands'][-1]['exit']==10 and all(c['exit']==0 for c in baseline['commands'][:-1])
 assert 'original_HAL=0 native_2048_guard_rejected_sessions=2 SOURCE_POLICY_CHECK_ONLY' in baseline['stdout']
 assert sha(BASE/'baseline-windows/frame-budget.exe')==baseline['binary_sha256']
 assert sha(BASE/'baseline-windows/platform_switch.cpp')==baseline['baseline_recorder_sha256']
 assert 'if(packets.size()>=2048)throw' in (BASE/'baseline-windows/platform_switch.cpp').read_text()
 rows=[]
 for name,digest in baseline['capture_sha256'].items():
  assert sha(ROOT/name)==digest,name
  with (ROOT/name).open(newline='') as stream:part=list(csv.DictReader(stream))
  assert len(part)==46 and [int(r['frame']) for r in part]==list(range(46));rows+=part
 total=sum(int(r['primary_draws']) for r in rows);peak=max(int(r['primary_draws']) for r in rows);over=sum(int(r['primary_draws'])>2048 for r in rows)
 assert (len(rows),total,peak,over)==(92,208808,4038,40)
 for kind in ('host','arm64'):
  result=profiles[kind];assert result['status']=='PASS' and all(c['exit']==0 for c in result['commands'])
  assert 'original_frames=92 primary_draws=208808 split_frames=40 max_frame=4038 max_batch=2048 submissions=132' in result['stdout']
  assert 'CPU_QUEUE_MODEL_ONLY NO_SWITCH_GPU' in result['stdout']
  assert sha(BASE/kind/'native-batch')==result['binary_sha256']
  for name,digest in result['capture_sha256'].items():assert sha(ROOT/name)==digest,name
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 for kind in ('host','arm64'):assert '-UNDEBUG' in profiles[kind]['commands'][0]['command']
 sdk=profiles['sdk'];assert sdk['status']=='COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN'
 assert all(c['exit']==0 for c in sdk['commands']) and not sdk['strong_undefined']
 assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for name,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/name)==digest,name
 assert len(sdk['link_inputs_sha256'])==183
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256']
 assert generated==read(ROOT/'build/matrixgame/e2e538-selection/sdk/source/native-engine-source.json')['files_sha256'] and len(generated)==255
 for name,digest in generated.items():assert sha(BASE/'sdk/source'/name)==digest,name
 symbols=(BASE/'sdk/symbols.log').read_text(encoding='utf-8')
 for symbol in ('CGame::Init(', 'CMatrixMap::PrepareMap(', 'DekoDevice::PresentNativeFrame(', 'DekoDevice::Impl::SubmitAndWait()', 'NativeDrawBatch::NeedsDrain('):assert symbol in symbols,symbol
 for kind,result in profiles.items():
  for name,digest in result['source_sha256'].items():assert sha(ROOT/name)==digest,(kind,name)
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
 assert output('rev-parse','HEAD')==previous['parent']
 assert output('rev-parse','HEAD',cwd=ROOT/'upstream/matrixgame')=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all',cwd=ROOT/'upstream/matrixgame')
 assert not list(BASE.rglob('*.nro'))
 names={p for result in profiles.values() for p in result['source_sha256']}
 names|={'tools/matrixgame_batch_audit.py','docs/matrixgame-init-sequence.md','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e539-batches.md'}
 for name in names:
  text=(ROOT/name).read_text(encoding='utf-8');assert not any(line.rstrip()!=line or line.startswith(('<<<<<<<','=======','>>>>>>>')) for line in text.splitlines()),name
 result=dict(status='E2E-5.39 BATCH POLICY / SDK PASS; SWITCH RUNTIME PARTIAL',profiles=profiles,
  source_sha256={p:sha(ROOT/p) for p in sorted(names)},measured_original_frames=92,measured_primary_draws=total,max_primary_draws=peak,frames_above_old_limit=over,
  policy_replay_submissions=132,packet_limit_per_batch=2048,byte_limit_per_batch=268435456,
  baseline_original_HAL_passed=True,baseline_native_guard_is_source_policy_analysis=True,native_GPU_baseline_executed=False,
  original_generated_files_unchanged=len(generated),sdk_reachable_link_inputs=len(sdk['link_inputs_sha256']),protected_verified=protected,hardware_inputs_verified=len(hardware),
  submit_completion_in_CPU_tests='EXPLICIT MODEL; NOT GPU EXECUTION',native_Init_executed=False,native_map_loaded=False,native_frame_presented=False,
  first_native_runtime_blocker='NOT OBSERVED ON SWITCH',Switch_hardware='NOT RUN / HARDWARE_PENDING',
  NRO_created=False,commit=False,push=False,P0='OPEN',GI_repeating_text_causality='NOT PROVEN',manual_publish_ENOENT='OPEN',Switch_hangs='OPEN',E2E534_HAL_retry='OPEN / FAIL',queue_loss_recovery='OPEN')
 (ROOT/'reports/matrixgame-e2e539-batches-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','measured_original_frames','max_primary_draws','policy_replay_submissions','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

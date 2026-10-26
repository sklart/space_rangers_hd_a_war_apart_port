"""Audit command-memory ownership/protocol, real SDK link, and protected inputs."""
import json,subprocess
from pathlib import Path
from matrixgame_command_checks import ROOT,BASE,sha
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def main():
 profiles={kind:read(BASE/kind/'result.json') for kind in ('host','arm64','sdk')}
 for kind,result in profiles.items():
  assert result['status']==('COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if kind=='sdk' else 'PASS'),kind
  assert all(c['exit']==0 for c in result['commands']),kind
  for name,digest in result['source_sha256'].items():assert sha(ROOT/name)==digest,(kind,name)
 for kind in ('host','arm64'):
  result=profiles[kind];assert 'growth_pages=4 failure_cases=6' in result['stdout']
  for marker in ('no_failed_submit','clear_rebind_before_free','batch_composition','command_pressure_drain','clear_pressure_drain','completion_failure_retains_pages','CPU_WRITER_QUEUE_MODEL_ONLY NO_SWITCH_GPU'):assert marker in result['stdout'],marker
  assert '-UNDEBUG' in result['commands'][0]['command']
  assert sha(BASE/kind/'native-command-memory')==result['binary_sha256']
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 sdk=profiles['sdk'];assert not sdk['strong_undefined'] and len(sdk['link_inputs_sha256'])==183
 assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for name,digest in sdk['link_inputs_sha256'].items():assert sha(ROOT/name)==digest,name
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(generated)==255
 previous_sdk=read(ROOT/'build/matrixgame/e2e539-batches/sdk/result.json')
 assert generated==read(ROOT/'build/matrixgame/e2e539-batches/sdk/source/native-engine-source.json')['files_sha256']
 for name,digest in generated.items():assert sha(BASE/'sdk/source'/name)==digest,name
 assert sha(BASE/'baseline/platform_switch.cpp')==previous_sdk['source_sha256']['port/matrixgame/native-engine/platform_switch.cpp']
 assert sha(ROOT/'build/matrixgame/e2e539-batches/sdk/matrixgame-native-entry.elf')==previous_sdk['elf_sha256']
 old=(BASE/'baseline/platform_switch.cpp').read_text();assert 'commands=dk::CmdBufMaker{device}.create()' in old and 'setCbAddMem' not in old and 'command_bytes=2*1024*1024' in old
 sdk_files={'deko3d.h':Path('C:/devkitPro/libnx/include/deko3d.h'),'deko3d.hpp':Path('C:/devkitPro/libnx/include/deko3d.hpp'),'desc':Path('C:/devkitPro/msys2/var/lib/pacman/local/deko3d-0.5.0-1/desc')}
 for name,path in sdk_files.items():assert sha(BASE/'baseline'/name)==sha(path),name
 assert '\n0.5.0-1\n' in (BASE/'baseline/desc').read_text()
 reference=(BASE/'baseline/dk_cmdbuf-v0.5.0.h').read_text();assert 'm_hasFlushFunc{false}' in reference
 reference=(BASE/'baseline/dk_cmdbuf-v0.5.0.cpp').read_text();assert 'if (!m_cbAddMem)' in reference and 'add-mem callback did not add enough command memory' in reference
 source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
 assert 'setCbAddMem(RefillCommands)' in source and source.count('submitCommands(')==2 and source.count('command_pages.FinishForSubmission(')==2
 assert 'batch.NeedsDrain(bytes) || command_pages.NeedsDrain()' in source
 symbols=(BASE/'sdk/symbols.log').read_text(encoding='utf-8')
 for symbol in ('CGame::Init(', 'CMatrixMap::PrepareMap(', 'DekoDevice::Impl::RefillCommands(', 'DekoDevice::Impl::CommandDriver::AttachDiscard(', 'DekoDevice::PresentNativeFrame('):assert symbol in symbols,symbol
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
 names|={'tools/matrixgame_command_audit.py','docs/matrixgame-init-sequence.md','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e540-command-memory.md'}
 for name in names:
  text=(ROOT/name).read_text(encoding='utf-8');assert not any(line.rstrip()!=line or line.startswith(('<<<<<<<','=======','>>>>>>>')) for line in text.splitlines()),name
 result=dict(status='E2E-5.40 COMMAND MEMORY POLICY / SDK PASS; SWITCH RUNTIME PARTIAL',profiles=profiles,
  source_sha256={p:sha(ROOT/p) for p in sorted(names)},baseline_sha256={p.name:sha(p) for p in (BASE/'baseline').iterdir() if p.is_file()},
  installed_deko3d='0.5.0-1',source_contract='https://github.com/devkitPro/deko3d/blob/v0.5.0/source/dk_cmdbuf.cpp',
  base_command_bytes=2097152,normal_command_quota=16777216,discard_reserve_bytes=16777216,max_command_backing_bytes=33554432,
  callback_exceptions_escape=False,failed_list_submission_forbidden=True,clear_rebind_before_free=True,
  ordinary_SDK_CmdBuf_auto_flush=False,
  own_allocation_failures='NULL/EXCEPTION CPU MODEL; NOT GPU OOM PROOF',command_pressure='CPU MODEL ONLY; original map command byte demand unmeasured',
  original_generated_files_unchanged=len(generated),sdk_reachable_link_inputs=len(sdk['link_inputs_sha256']),protected_verified=protected,hardware_inputs_verified=len(hardware),
  Windows_HAL='NOT RERUN; private Switch code only; latest complete original run E2E-5.39',
  native_Init_executed=False,native_map_loaded=False,native_frame_presented=False,first_native_runtime_blocker='NOT OBSERVED ON SWITCH',Switch_hardware='NOT RUN / HARDWARE_PENDING',
  NRO_created=False,commit=False,push=False,P0='OPEN',GI_repeating_text_causality='NOT PROVEN',manual_publish_ENOENT='OPEN',Switch_hangs='OPEN',E2E534_HAL_retry='OPEN / FAIL',queue_loss_recovery='OPEN')
 (ROOT/'reports/matrixgame-e2e540-command-memory-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','installed_deko3d','max_command_backing_bytes','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

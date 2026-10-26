"""Audit original/native scalar proof, exact SDK ordinals and protected inputs."""
import json,hashlib,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e529-stored-state'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 results={}
 for kind in ('host','arm64','windows','sdk'):
  j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
  for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
  results[kind]=j
  if kind!='sdk':
   assert 'argb_cases=1024 shader_uniforms=1328_exact restore/reset=3' in j['stdout']
   assert 'CPU_SCALAR_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN' in j['stdout']
   assert sha(BASE/kind/('stored-state.exe' if kind=='windows' else 'stored-state'))==j['binary_sha256']
  if kind in ('host','arm64'):
   assert 'restores=9 capture_OOM=2 COW_OOM=1 restore_allocations=0' in j['lease_stdout']
   assert sha(BASE/kind/'state-lease')==j['lease_binary_sha256']
 assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
 assert '-UNDEBUG' in results['host']['commands'][0]['command'] and '-UNDEBUG' in results['arm64']['commands'][0]['command']
 hal=read(BASE/'materials/windows/result.json');assert hal['status']=='PASS' and all(c['exit']==0 for c in hal['commands'])
 assert 'fixed vertex cases=84 GLSL_feedback=84 D3D9_ProcessVertices=80' in hal['stdout']
 assert 'D3D9_differential=1476 mismatches=0' in hal['stdout'] and 'GLSL_differential=1476 GLSL_mismatches=0' in hal['stdout']
 for n,h in hal['input_sha256'].items():assert sha(Path(n) if Path(n).is_absolute() else ROOT/n)==h,n
 sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 symbols=(BASE/'sdk/symbols.log').read_text()
 for name in ('CMatrixMap::CheckLostDevice(', 'CMatrixMap::ReleasePoolDefaultResources(', 'CDeviceState::StoreState(', 'CDeviceState::RestoreState(', 'DekoDevice::StoredStates()'):assert name in symbols,name
 original=ROOT/'upstream/matrixgame/MatrixLib/3G/DeviceState.cpp';s=original.read_text();renders=re.findall(r'\bD3DRS_\w+',s);samplers=re.findall(r'\bD3DSAMP_\w+',s)
 sdk_types=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared/d3d9types.h');ids={n:int(v) for n,v in re.findall(r'\b(D3DRS_\w+|D3DSAMP_\w+)\s*=\s*(\d+)\s*,',sdk_types.read_text())}
 layout=(ROOT/'port/matrixgame/native-engine/native_state_layout.hpp').read_text();words=re.findall(r'\{(\d+),(0x[0-9a-f]+|\d+)u,"(\w+)"\}',layout)
 assert len(renders)==103 and len(samplers)==13 and len(words)==116
 for (numeric,initial,name),expected in zip(words,renders+samplers):assert name==expected.split('_',1)[1] and int(numeric)==ids[expected],expected
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(generated)==252
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
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 parent=output('rev-parse','HEAD');assert parent==previous['parent'];up=ROOT/'upstream/matrixgame'
 assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not output('status','--porcelain','--untracked-files=all',cwd=up)
 names=sorted(set(n for j in results.values() for n in j['source_sha256']))+['tools/matrixgame_stored_state_audit.py','reports/matrixgame-e2e529-stored-state.md','docs/matrixgame-readiness-plan.md']
 subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,check=True)
 for n in names:
  if not n.startswith('build'):
   assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.29 STORED SCALAR STATE SOFTWARE PASS / FULL NATIVE INIT PARTIAL','results':results,'windows_HAL':hal,
  'source_sha256':{n:sha(ROOT/n) for n in names},'sdk_types_sha256':sha(sdk_types),'original_DeviceState_sha256':sha(original),
  'parent':parent,'original_generated_files_unchanged':252,'protected_verified':checked,'hardware_inputs_verified':len(hardware),
  'stored_guard_closed':'DekoDevice implements NativeStoredStateBackend on its authoritative NativeStateStore',
  'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
  'next_conditional_guard':'CMatrixMap::Draw [MMFLAG_FOG_ON] -> map_device::RasterCaps -> map raster caps backend unavailable',
  'next_stencil_guard':'CMatrixMap::DrawShadows -> SHADEMODE=FLAT -> native flat shading not implemented; StencilCaps/pipeline also pending',
  'first_runtime_guard_of_native_Init':'NOT OBSERVED; no new Switch execution','complete_map_reset_runtime':'NOT RUN',
  'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e529-stored-state-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','original_generated_files_unchanged','protected_verified','hardware_inputs_verified','next_conditional_guard')}))
if __name__=='__main__':main()

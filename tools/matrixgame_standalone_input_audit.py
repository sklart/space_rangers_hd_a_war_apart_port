"""Audit standalone-only change, original Init evidence and protected inputs."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e535-standalone-input'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 sdk=read(BASE/'sdk/result.json');assert sdk['status']=='COMPILE/LINK PASS; NATIVE HID RUNTIME NOT RUN'
 assert all(c['exit']==0 for c in sdk['commands']) and not sdk['strong_undefined']
 for p,h in sdk['source_sha256'].items():assert sha(ROOT/p)==h,p
 for p,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/p)==h,p
 assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 graph=(BASE/'sdk/link-graph.log').read_text();reachable=set()
 for label in re.findall(r'\[label="((?:\\.|[^"\\])*)"',graph):
  p=(BASE/'sdk'/Path(json.loads('"'+label+'"'))).resolve()
  if p.is_relative_to(ROOT) and p.suffix in ('.o','.obj','.a') and p.is_file():reachable.add(str(p.relative_to(ROOT)))
 assert reachable==set(sdk['link_inputs_sha256'])
 assert sdk['original_generated_files_unchanged']==255 and sdk['borrowed_input_provider_unchanged']
 previous=read(ROOT/'build/matrixgame/e2e533-form/windows/result.json')
 assert previous['status']=='PASS';log=(ROOT/'build/matrixgame/e2e533-form/windows/runtime.log').read_text()
 for marker in ('RobotPreload complete','map storage loaded','PrepareMap complete','REAL_FORM_LOOP draws=8 elapsed=125'):assert log.count(marker)==2,marker
 assert sha(ROOT/'build/matrixgame/e2e533-form/windows/form-loop.exe')==previous['binary_sha256']
 # Keep the genuine HAL diagnostic failure open and byte-identical.
 hal=read(ROOT/'build/matrixgame/e2e534-reset-owners/windows/result.json')
 assert hal['status']=='FAIL' and hal['commands'][-1]['exit']==10 and 'cases=15 mismatches=2' in hal['stdout']
 assert sha(ROOT/'build/matrixgame/e2e534-reset-owners/windows/oracle.exe')==hal['binary_sha256']
 for p,h in hal['source_sha256'].items():assert sha(ROOT/p)==h,p
 # The original scalar recovery has no D3D state-block creation/ownership path.
 up=ROOT/'upstream/matrixgame'
 tokens=re.compile(r'\b(?:CreateStateBlock|BeginStateBlock|EndStateBlock)\b')
 scanned=[]
 for p in up.rglob('*'):
  if p.is_file() and p.suffix in ('.cpp','.hpp'):
   assert not tokens.search(p.read_text(encoding='utf-8',errors='replace')),str(p)
   scanned.append(str(p.relative_to(up)))
 state=(up/'MatrixLib/3G/DeviceState.cpp').read_text();assert 'GetTexture(' not in state and 'SetTexture(' not in state
 old=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');checked=0
 for n,v in old['protected'].items():
  if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
 for n,v in old['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
 primary=old['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==old['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 for n,h in read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():assert sha(up/'MatrixGame/CFG'/n)==h,n
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 assert output('rev-parse','HEAD')==old['parent']
 assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
 assert not output('status','--porcelain','--untracked-files=all',cwd=up)
 names=set(sdk['source_sha256'])|{'tools/matrixgame_standalone_input_checks.py','tools/matrixgame_standalone_input_audit.py',
  'reports/matrixgame-e2e535-standalone-input.md','docs/matrixgame-init-sequence.md','docs/matrixgame-readiness-plan.md','docs/matrixgame-native-providers.md'}
 subprocess.run([git,'diff','--check','--',*sorted(names)],cwd=ROOT,check=True)
 for n in names:assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.35 STANDALONE INPUT COMPILE/LINK PASS; SWITCH RUNTIME PARTIAL','sdk':sdk,
  'source_sha256':{n:sha(ROOT/n) for n in sorted(names)},'libnx_headers_sha256':{str(p):sha(p) for p in [Path('C:/devkitPro/libnx/include/switch/runtime/pad.h'),Path('C:/devkitPro/libnx/include/switch/services/hid.h')]},
  'sdk_reachable_own_link_inputs':len(reachable),'protected_verified':checked,'hardware_inputs_verified':len(hardware),
  'original_state_block_scan':{'source_files_checked':len(scanned),'create_begin_end_state_block_occurrences':0,'scalar_state_has_texture_bindings':False},
  'previous_Windows_full_Init_evidence':{'path':'build/matrixgame/e2e533-form/windows/result.json','sha256':sha(ROOT/'build/matrixgame/e2e533-form/windows/result.json'),'actual_sessions':2,'new_execution':False},
  'E2E534_HAL_retry':'OPEN / FAIL: 2 mismatches; diagnostic source/binary verified unchanged',
  'HOST':'NOT RUN this stage; pure Switch entry change','ARM64_QEMU':'NOT RUN; Horizon HID IPC unavailable','WINDOWS_HAL':'NOT RUN this stage; previous evidence separately verified',
  'Switch_hardware':'NOT RUN / HARDWARE_PENDING','native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,
  'first_native_runtime_blocker':'NOT OBSERVED','NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e535-standalone-input-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','sdk_reachable_own_link_inputs','protected_verified','hardware_inputs_verified','original_state_block_scan')},ensure_ascii=False))
if __name__=='__main__':main()

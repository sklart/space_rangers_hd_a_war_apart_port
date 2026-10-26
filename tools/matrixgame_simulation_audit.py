"""Audit actual original Takt retention/execution and protected inputs."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_vector_object_overrides import function
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e532-simulation'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 sdk=read(BASE/'sdk/result.json');win=read(BASE/'windows/result.json')
 assert sdk['status']=='COMPILE/LINK PASS; RUNTIME NOT RUN' and win['status']=='PASS'
 for j in (sdk,win):
  assert all(c['exit']==0 for c in j['commands'])
  for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,n
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 assert not sdk['strong_undefined'] and sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 assert sha(BASE/'windows/simulation.exe')==win['binary_sha256']
 assert win['real_Init_Takt_Draw_Deinit_sessions']==2 and win['real_Takt_steps']==20
 assert win['stdout'].count('REAL_SIMULATION steps=10 elapsed=250 original_Takt_and_Draw=returned')==2
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256']
 previous=read(ROOT/'build/matrixgame/e2e531-stencil/sdk/source/native-engine-source.json')['files_sha256']
 assert len(generated)==253 and set(generated)-set(previous)=={'MatrixGame/src/OriginalNativeTakt.cpp'}
 for n,h in previous.items():assert generated[n]==h,n
 for n,h in generated.items():assert sha(BASE/'sdk/source'/n)==h,n
 original=function((ROOT/'upstream/matrixgame/MatrixGame/src/MatrixFormGame.cpp').read_text(encoding='utf-8'),'void CFormMatrixGame::Takt(int step)')
 candidate=function((BASE/'sdk/source/MatrixGame/src/OriginalNativeTakt.cpp').read_text(encoding='utf-8'),'void TaktOriginalNativeGame(int step)')
 assert candidate.replace('void TaktOriginalNativeGame(int step)','void CFormMatrixGame::Takt(int step)',1)==original
 symbols=(BASE/'sdk/symbols.log').read_text(encoding='utf-8')
 for name in ('TaktOriginalNativeGame(int)','CMatrixMapLogic::Takt(int)','CMatrixMap::Takt(int)','CMatrixSideUnit::LogicTakt(int)','CGame::Init('):assert name in symbols,name
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
 assert output('rev-parse','HEAD')==old['parent'];up=ROOT/'upstream/matrixgame'
 assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not output('status','--porcelain','--untracked-files=all',cwd=up)
 names=sorted(set(sdk['source_sha256'])|set(win['source_sha256'])|{'tools/matrixgame_simulation_audit.py','reports/matrixgame-e2e532-simulation.md','docs/matrixgame-readiness-plan.md'})
 subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,check=True)
 for n in names:assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.32 ORIGINAL TAKT SOFTWARE PROGRESS / NATIVE LOOP PARTIAL',
         'SDK':sdk,'WINDOWS_HAL':win,'HOST_ASan_UBSan':'NOT RUN for this simulation delta',
         'ARM64_QEMU':'NOT RUN for this simulation delta','SWITCH_HARDWARE':'NOT RUN / HARDWARE_PENDING',
         'original_Takt_body_identical':True,'original_generated_files_unchanged':252,
         'protected_verified':checked,'hardware_inputs_verified':len(hardware),
         'source_sha256':{n:sha(ROOT/n) for n in names},
         'frame_sha256':{p.name:sha(p) for p in (BASE/'windows').glob('map-frame-*.png')},
         'native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,
         'native_loop_complete':False,'next_source_dependency':'original CFormMatrixGame Enter/Leave/Mouse/Keyboard/SystemEvent and L3GRun platform pump',
         'first_native_runtime_blocker':'NOT OBSERVED','commit':False,'push':False,'NRO_created':False,
         'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e532-simulation-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','original_Takt_body_identical','protected_verified','hardware_inputs_verified','first_native_runtime_blocker')}))
if __name__=='__main__':main()

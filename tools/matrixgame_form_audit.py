"""Audit native original form/loop, real HAL recovery and protected inputs."""
import hashlib,json,re,subprocess
from pathlib import Path
from matrixgame_vector_object_overrides import function
from matrixgame_form_overrides import KEYS
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e533-form'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 profiles={kind:read(BASE/kind/'result.json') for kind in ('sdk','windows','host','arm64','state/host','state/arm64')}
 noncompiled_drift={}
 for name,j in profiles.items():
  assert j['status']==('COMPILE/LINK PASS; RUNTIME NOT RUN' if name=='sdk' else 'PASS'),name
  assert all(c['exit']==0 for c in j['commands']),name
  drift={n for n,h in j['source_sha256'].items() if sha(ROOT/n)!=h}
  if drift:
   # The older scalar runner also fingerprints SDK-only launch/build metadata
   # even in its direct WSL compilation modes. Prove that changed metadata is
   # outside the actual compiler dependency graph instead of rerunning CPU
   # suites solely for subsequent Switch launcher changes.
   allowed={'port/matrixgame/native-engine/CMakeLists.txt','port/matrixgame/native-engine/platform_switch.hpp','tests/probe_matrixgame_native_entry.cpp'}
   assert name.startswith('state/') and drift<=allowed,(name,drift)
   command=j['commands'][0]['command'][:];at=command.index('-o');del command[at:at+2];command+=['-MM']
   process=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');assert process.returncode==0,process.stderr
   (BASE/name/'include-deps.log').write_text(process.stdout,encoding='utf-8')
   for n in drift:assert str(ROOT/n).replace('\\','/').replace('D:/','/mnt/d/') not in process.stdout,n
   noncompiled_drift[name]=sorted(drift)
 sdk=profiles['sdk'];assert not sdk['strong_undefined']
 assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 assert not any(n.endswith(('OriginalNativeTakt.cpp.obj','OriginalWindowsFrame.cpp.obj','matrixgame_native_font_probe.cpp.obj')) for n in sdk['link_inputs_sha256'])
 graph=(BASE/'sdk/link-graph.log').read_text(encoding='utf-8');reachable=set()
 for label in re.findall(r'\[label="((?:\\.|[^"\\])*)"',graph):
  p=(BASE/'sdk'/Path(json.loads('"'+label+'"'))).resolve()
  if p.is_relative_to(ROOT) and p.suffix in ('.o','.obj','.a') and p.is_file():reachable.add(str(p.relative_to(ROOT)))
 assert reachable==set(sdk['link_inputs_sha256'])
 symbols=(BASE/'sdk/symbols.log').read_text(encoding='utf-8')
 for name in ('CGame::Init(', 'CMatrixMap::CalcVis(', 'CMatrixMap::CalcMapGroupVisibility(', 'CFormMatrixGame::Takt(int)', 'CFormMatrixGame::Draw()', 'CFormMatrixGame::MouseKey(', 'CFormMatrixGame::Keyboard(', 'CFormMatrixGame::SystemEvent(', 'RunOriginalNativeLoop()', 'DekoDevice::DebugFontSession()', 'DekoDevice::AcquireNativeFrame()'):assert name in symbols,name
 win=profiles['windows'];assert sha(BASE/'windows/form-loop.exe')==win['binary_sha256']
 for marker in ('REAL_FORM_LOOP draws=8 elapsed=125 Enter/Pause/Escape/scroll/MouseMove/F9_guard=PASS focus_release_recreate=PASS', 'SDK_SLOT_ORACLE raw_light_material=exact raw_enable=exact undefined_HRESULT_preserved=PASS logical_type0_capture=PASS'):
  assert win['stdout'].count(marker)==2,marker
 for name in ('host','arm64'):
  j=profiles[name];assert 'original L3GRun scheduler PASS' in j['stdout'] and 'CPU FIXTURES NO_MAP NO_GPU NO_SWITCH_HARDWARE' in j['stdout'];assert sha(BASE/name/'original-loop')==j['binary_sha256']
  state=profiles['state/'+name];assert 'original CDeviceState/native scalar PASS' in state['stdout'] and 'absent_light=original failed_recapture=guarded' in state['stdout']
  assert 'capture_OOM=2 COW_OOM=1' in state['lease_stdout'];assert sha(BASE/'state'/name/'stored-state')==state['binary_sha256'];assert sha(BASE/'state'/name/'state-lease')==state['lease_binary_sha256']
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 assert '-fsanitize=address,undefined' in profiles['state/host']['commands'][0]['command']
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];previous=read(ROOT/'build/matrixgame/e2e532-simulation/sdk/source/native-engine-source.json')['files_sha256']
 assert len(generated)==255 and set(generated)-set(previous)=={'MatrixGame/src/OriginalNativeForm.cpp','MatrixLib/3G/OriginalNativeLoop.cpp'}
 changed={n for n,h in previous.items() if generated[n]!=h};assert changed=={'MatrixLib/3G/DeviceState.cpp'}
 for n,h in generated.items():assert sha(BASE/'sdk/source'/n)==h,n
 original=(ROOT/'upstream/matrixgame/MatrixGame/src/MatrixFormGame.cpp').read_text(encoding='utf-8');form=(BASE/'sdk/source/MatrixGame/src/OriginalNativeForm.cpp').read_text(encoding='utf-8')
 identical=[]
 for signature in ('CFormMatrixGame::CFormMatrixGame()', 'CFormMatrixGame::~CFormMatrixGame()', 'void CFormMatrixGame::Leave(void)', 'void CFormMatrixGame::Takt(int step)'):
  assert function(original,signature)==function(form,signature),signature;identical.append(signature)
 # Handler logic is unchanged after literal protocol/type substitutions.
 for signature in ('void CFormMatrixGame::Keyboard(', 'void CFormMatrixGame::MouseKey('):
  expected=function(original,signature)
  for name,value in KEYS.items():expected=re.sub(r'\b'+name+r'\b',str(value),expected)
  expected=re.sub(r'\bDWORD\b','std::uint32_t',expected);expected=re.sub(r'\bTRUE\b','true',expected);expected=re.sub(r'\bFALSE\b','false',expected)
  if signature.startswith('void CFormMatrixGame::Keyboard'):
   assert expected.count('ZeroMemory(&sb, sizeof(SSpecialBot));')==2
   expected=expected.replace('ZeroMemory(&sb, sizeof(SSpecialBot));','::srhd_awa::matrixgame::ZeroTrivialBytes(&sb, sizeof(SSpecialBot));')
  assert expected==function(form,signature),signature;identical.append(signature+' [protocol literals]')
 expected=function(original,'void CFormMatrixGame::MouseMove(').replace('ClientToScreen(g_Wnd, (LPPOINT)&p);','native_form::ClientPointToScreen(p);')
 assert expected==function(form,'void CFormMatrixGame::MouseMove(')
 # Reconstruct the original L3GRun after reversing the explicit platform seams.
 loop=function((BASE/'sdk/source/MatrixLib/3G/OriginalNativeLoop.cpp').read_text(encoding='utf-8'),'int RunOriginalNativeLoop()')
 messages='''        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg); // Effectively calls L3GWndProc() handler
        }'''
 for a,b in [('int RunOriginalNativeLoop()','int L3GRun()'),('using clock = native_loop::Clock;','using clock = std::chrono::high_resolution_clock;'),('native_loop::Now()','clock::now()'),('native_loop::Idle();','std::this_thread::yield();'),('native_loop::DrawCurrentForm();','g_FormCur->Draw();'),('srhd_awa::matrixgame::platform3g::ReadyDevice().AvailableTextureBytes()','g_D3DD->GetAvailableTextureMem()'),('std::min<std::int64_t>(100, to_milliseconds(delta_time).count())','std::min(100LL, to_milliseconds(delta_time).count())'),('        native_loop::Pump();',messages),('    fps_counter<100> fps;\n','    fps_counter<100> fps;\n    MSG msg;')]:loop=loop.replace(a,b)
 original_loop=function((ROOT/'upstream/matrixgame/MatrixLib/3G/3g.cpp').read_text(encoding='utf-8'),'int L3GRun()')
 assert re.sub(r'\s+',' ',loop)==re.sub(r'\s+',' ',original_loop)
 assert 'BeginNativeFrame(' not in (ROOT/'port/matrixgame/native-engine/native_loop_switch.cpp').read_text()
 old=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');checked=0
 for n,v in old['protected'].items():
  if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
 for n,v in old['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
 primary=old['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==old['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 for n,h in read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 assert output('rev-parse','HEAD')==old['parent'];up=ROOT/'upstream/matrixgame'
 assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not output('status','--porcelain','--untracked-files=all',cwd=up)
 names=set(n for j in profiles.values() for n in j['source_sha256'])
 names.update(['tools/matrixgame_form_audit.py','tools/matrixgame_form_state_checks.py','port/matrixgame/native-engine/native_frame_clock.cpp','reports/matrixgame-e2e533-form.md','docs/matrixgame-readiness-plan.md','docs/matrixgame-native-providers.md'])
 subprocess.run([git,'diff','--check','--',*sorted(names)],cwd=ROOT,check=True)
 for n in names:
  if Path(n).parts[0]!='build':assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.33 ORIGINAL FORM/LOOP SOFTWARE PASS / SWITCH RUNTIME PARTIAL','profiles':profiles,
  'original_loop_algorithm_preserved':True,'original_form_body_checks':identical,'previous_generated_files_unchanged':252,'changed_generated':sorted(changed),
  'scalar_runner_noncompiled_metadata_drift_verified_by_compiler':noncompiled_drift,
  'sdk_reachable_own_link_inputs':len(reachable),'source_sha256':{n:sha(ROOT/n) for n in sorted(names)},
  'protected_verified':checked,'hardware_inputs_verified':len(hardware),'frames_sha256':{p.name:sha(p) for p in (BASE/'windows').glob('map-frame-*.png')},
  'baseline_failure_artifacts':{str(p.relative_to(BASE)):sha(p) for p in (BASE/'focus-light-baseline').iterdir() if p.is_file()},
  'native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
  'OS_HID_input_tested':False,'native_screenshot':'UNSUPPORTED explicitly at CFormMatrixGame::Keyboard F9; optional, not first-map requirement',
  'first_native_runtime_blocker':'NOT OBSERVED','remaining_first_map_gates':'real native allocation/upload/frame/resource recovery on Switch; audit remaining actually-used render paths',
  'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e533-form-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','sdk_reachable_own_link_inputs','protected_verified','hardware_inputs_verified','first_native_runtime_blocker')}))
if __name__=='__main__':main()

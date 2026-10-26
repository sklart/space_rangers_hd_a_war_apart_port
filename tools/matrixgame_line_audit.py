"""Audit actual minimap line candidate while preserving hardware/open-mode gates."""
import hashlib,json,re,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e537-lines'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 names=('windows','host','arm64','map-state/windows','map-state/host','map-state/arm64','sdk')
 profiles={n:read(BASE/n/'result.json') for n in names}
 for n,j in profiles.items():
  assert j['status']==('COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if n=='sdk' else 'PASS'),n
  assert all(c['exit']==0 for c in j['commands']),n
  for p,h in j['source_sha256'].items():assert sha(ROOT/p)==h,(n,p)
  for p,h in j.get('capture_sha256',{}).items():assert sha(ROOT/p)==h,(n,p)
  exe={'windows':'line-oracle.exe','host':'line-state','arm64':'line-state','map-state/windows':'actual-draw.exe','map-state/host':'replay','map-state/arm64':'replay','sdk':'matrixgame-native-entry.elf'}[n]
  assert sha(BASE/n/exe)==j.get('binary_sha256',j.get('elf_sha256')),n
 win=profiles['windows'];assert 'RAW_LINE_HAL_GLSL cases=398 strips=64 degenerate=14 last0_mismatches=0 last1_mismatches=0 max_delta=0' in win['stdout']
 assert 'ACTUAL_MINIMAP_LINE_HAL_GLSL cases=20 pixels=9600000 mismatches=0 max_delta=0' in win['stdout']
 game=profiles['map-state/windows'];assert 'ACTUAL_MAP_HAL_RESULT=0 NATIVE_STATE_REJECTED_SESSIONS=0' in game['stdout']
 for marker in ('REAL_FORM_LOOP draws=8 elapsed=125','focus_release_recreate=PASS','actual gamma/window readback restored; device/timer leases empty','RobotPreload complete','map storage loaded','PrepareMap complete'):assert game['stdout'].count(marker)==2,marker
 baseline=read(ROOT/'build/matrixgame/e2e536-actual-draw/windows/result.json');assert baseline['status']=='FAIL' and baseline['commands'][-1]['exit']==10
 assert sha(ROOT/'build/matrixgame/e2e536-actual-draw/windows/actual-draw.exe')==baseline['binary_sha256']
 assert sorted(baseline['capture_sha256'].values())==sorted(game['capture_sha256'].values())
 for p,h in baseline['capture_sha256'].items():assert sha(ROOT/p)==h,p
 for n in ('host','arm64'):
  j=profiles[n];assert 'native line state PASS' in j['stdout'] and 'scalar/COW restore preserves FALSE' in j['stdout'] and 'AA/point/scissor guards retained' in j['stdout']
  assert 'draws=33690 unique=16574 failures=0' in profiles['map-state/'+n]['stdout']
  assert not any(s in j['stdout']+profiles['map-state/'+n]['stdout'] for s in ('ERROR: AddressSanitizer','runtime error:','LeakSanitizer:'))
 for n in ('host','map-state/host'):assert '-fsanitize=address,undefined' in profiles[n]['commands'][0]['command']
 geometry=[];line_count=0
 for path in sorted((BASE/'map-state/windows').glob('line-geometry-*.bin')):
  data=path.read_bytes();magic,version,size,count=struct.unpack_from('<4I',data);assert (magic,version,size,count)==(0x374c524d,1,2808,10);offset=16
  for i in range(count):
   state=data[offset:offset+size];offset+=size
   primitives,stride,width,height,fmt,byte_count=struct.unpack_from('<6I',data,offset);offset+=24
   assert (primitives,stride,width,height,fmt,byte_count)==(4,16,800,600,21,80)
   assert struct.unpack_from('<I',state)[0]==3 and struct.unpack_from('<I',state,8)[0]==4 and struct.unpack_from('<I',state,32+5*4)[0]==1
   assert struct.unpack_from('<4I2f',state,1852)==(13,442,145,145,0.,1.)
   vertices=struct.unpack_from('<20f',data,offset);assert vertices[:4]==vertices[-4:];offset+=byte_count;line_count+=1
  assert offset==len(data);geometry.append({'path':str(path.relative_to(ROOT)),'sha256':sha(path),'actual_cases':count})
 assert len(geometry)==2 and line_count==20
 sdk=profiles['sdk'];assert not sdk['strong_undefined']
 for p,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/p)==h,p
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(generated)==255
 assert generated==read(ROOT/'build/matrixgame/e2e536-actual-draw/sdk/source/native-engine-source.json')['files_sha256']
 for p,h in generated.items():assert sha(BASE/'sdk/source'/p)==h,p
 symbols=(BASE/'sdk/symbols.log').read_text()
 for name in ('CGame::Init(', 'CFormMatrixGame::Draw()', 'DekoDevice::NativeDrawUP(', 'dkCmdBufSetLineWidth', 'dkCmdBufSetLineStipple'):assert name in symbols,name
 source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text();assert 'commands.setLineWidth(1.f);commands.setLineStipple(false,1,0xffff);' in source
 # The preliminary /Fo issue did not change the historical expected pixels.
 fog=read(ROOT/'build/matrixgame/e2e530-fog/windows/result.json');assert sha(ROOT/'build/matrixgame/e2e530-fog/windows/fog.golden')==fog['golden_sha256']
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
 paths={p for j in profiles.values() for p in j['source_sha256']}|{'tools/matrixgame_actual_draw_checks.py','tools/matrixgame_line_checks.py','tools/matrixgame_line_audit.py','reports/matrixgame-e2e537-lines.md','docs/matrixgame-readiness-plan.md','docs/matrixgame-init-sequence.md','docs/matrixgame-native-providers.md'}
 subprocess.run([git,'diff','--check','--',*sorted(paths)],cwd=ROOT,check=True)
 for p in paths:assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/p).read_text(encoding='utf-8').splitlines()),p
 assert not list(BASE.rglob('*.nro'))
 hal=read(ROOT/'build/matrixgame/e2e534-reset-owners/windows/result.json');assert hal['status']=='FAIL' and 'cases=15 mismatches=2' in hal['stdout']
 result={'status':'E2E-5.37 ORIGINAL MINIMAP ALIASED LINE SOFTWARE PASS / SWITCH RUNTIME PARTIAL','profiles':profiles,'geometry':geometry,
  'baseline_capture_payloads_byte_identical':True,'baseline_failure_preserved':True,'actual_map_draws':33690,'unique_snapshots':16574,'synthetic_line_cases':398,'actual_line_cases':20,'actual_RGBA_pixels':9600000,'max_delta':0,
  'sdk_reachable_own_link_inputs':len(sdk['link_inputs_sha256']),'original_generated_files_unchanged':len(generated),'source_sha256':{p:sha(ROOT/p) for p in sorted(paths)},'protected_verified':checked,'hardware_inputs_verified':len(hardware),
  'unsupported_line_modes':['LASTPIXEL=FALSE','ANTIALIASEDLINEENABLE=TRUE'],'first_source_confirmed_minimap_blocker':'SOFTWARE CLOSED for original LASTPIXEL=TRUE; native GPU hardware untested',
  'first_native_runtime_blocker':'NOT OBSERVED ON SWITCH','native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
  'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN','E2E534_HAL_retry':'OPEN / FAIL',
  'initial_probe_output_issue':'intermediate OBJ output in historical e2e530 build directory; corrected; historical fog golden hash verified unchanged'}
 result['frame_sha256']={p.name:sha(p) for p in (BASE/'map-state/windows').glob('map-frame-*.png')}
 (ROOT/'reports/matrixgame-e2e537-lines-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','actual_map_draws','actual_line_cases','actual_RGBA_pixels','sdk_reachable_own_link_inputs','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

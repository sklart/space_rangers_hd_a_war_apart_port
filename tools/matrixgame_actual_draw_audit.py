"""Audit a reproduced real-map native blocker; never relabel it functional PASS."""
import bisect,hashlib,json,re,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e536-actual-draw'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 profiles={n:read(BASE/n/'result.json') for n in ('windows','host','arm64','sdk')}
 for n,j in profiles.items():
  if n=='sdk':assert j['status']=='COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' and all(c['exit']==0 for c in j['commands'])
  else:assert j['status']=='FAIL' and j['commands'][-1]['label']=='runtime' and j['commands'][-1]['exit']==10 and all(c['exit']==0 for c in j['commands'][:-1])
  for p,h in j['source_sha256'].items():assert sha(ROOT/p)==h,(n,p)
  for p,h in j['capture_sha256'].items():assert sha(ROOT/p)==h,(n,p)
 win=profiles['windows'];assert sha(BASE/'windows/actual-draw.exe')==win['binary_sha256']
 assert 'ACTUAL_MAP_HAL_RESULT=0 NATIVE_STATE_REJECTED_SESSIONS=2' in win['stdout']
 for marker in ('REAL_FORM_LOOP draws=8 elapsed=125','focus_release_recreate=PASS','actual gamma/window readback restored; device/timer leases empty','RobotPreload complete','map storage loaded','PrepareMap complete'):
  assert win['stdout'].count(marker)==2,marker
 assert len(win['capture_sha256'])==2 and win['stdout'].count('NATIVE_STATE_REJECT unique=2 reason=native draw requires line rasterization/LASTPIXEL')==2
 for n in ('host','arm64'):
  j=profiles[n];assert sha(BASE/n/'replay')==j['binary_sha256']
  assert 'draws=33690 unique=16574 failures=1' in j['stdout'] and 'REJECT unique=4 reason=native draw requires line rasterization/LASTPIXEL' in j['stdout']
  assert not any(s in j['stdout'] for s in ('ERROR: AddressSanitizer','runtime error:','LeakSanitizer:'))
 assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
 sdk=profiles['sdk'];assert not sdk['strong_undefined'] and sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for p,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/p)==h,p
 assert sdk['original_generated_files_unchanged']==255
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256']
 assert generated==read(ROOT/'build/matrixgame/e2e535-standalone-input/sdk/source/native-engine-source.json')['files_sha256']
 for p,h in generated.items():assert sha(BASE/'sdk/source'/p)==h,p
 # Resolve each failing actual caller from the current executable's map/ASLR.
 maptext=(BASE/'windows/actual-draw.map').read_text(errors='replace');preferred=int(re.search(r'Preferred load address is ([0-9A-Fa-f]+)',maptext)[1],16)
 symbols=[]
 for line in maptext.splitlines():
  m=re.match(r'\s+0001:[0-9A-Fa-f]+\s+(\S+)\s+([0-9A-Fa-f]{8})\s',line)
  if m:symbols.append((int(m[2],16),m[1]))
 symbols.sort();addresses=[v for v,n in symbols];resolved=[]
 for line in win['stdout'].splitlines():
  m=re.match(r'NATIVE_REJECT_CALLER topology=3 fvf=00000004 lastpixel=1 image=([0-9A-Fa-f]+) stack:(.*)',line)
  if not m:continue
  frames=[]
  for word in m[2].split():
   value=int(word,16)-int(m[1],16)+preferred;i=bisect.bisect_right(addresses,value)-1
   if i>=0 and value-addresses[i]<0x10000:frames.append({'preferred_address':hex(value),'symbol':symbols[i][1],'offset':hex(value-addresses[i])})
  assert any('?Draw@CMinimap@@' in f['symbol'] for f in frames) and any('?DrawPrimitiveUP@map_device' in f['symbol'] for f in frames)
  resolved.append(frames)
 assert len(resolved)==4 and resolved==read(BASE/'windows/caller-resolution.json')
 # Independently count the binary payloads and the exact rejected topology.
 captures=[];total=unique=line_draws=line_states=0
 for p in sorted((BASE/'windows').glob('draw-states-*.bin')):
  data=p.read_bytes();magic,version,size,count=struct.unpack_from('<4I',data);assert (magic,version,size)==(0x3647524d,1,2808) and len(data)==16+count*(size+8)
  draws=lines=0;fvfs=set()
  for i in range(count):
   offset=16+i*(size+8);topology,indexed,fvf,bound,present,depth,msaa,feedback=struct.unpack_from('<8I',data,offset);frequency=struct.unpack_from('<Q',data,offset+size)[0]
   assert frequency and msaa==0 and feedback==0;draws+=frequency;fvfs.add(fvf)
   if topology in (2,3):
    assert topology==3 and fvf==4 and struct.unpack_from('<I',data,offset+32+5*4)[0]==1
    assert struct.unpack_from('<4I2f',data,offset+1852)==(13,442,145,145,0.,1.)
    lines+=frequency;line_states+=1
  total+=draws;unique+=count;line_draws+=lines;captures.append({'path':str(p.relative_to(ROOT)),'draws':draws,'unique':count,'line_draws':lines,'fvf':sorted(fvfs),'sha256':sha(p)})
 assert (total,unique,line_draws,line_states)==(33690,16574,20,4)
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
 names=set(n for j in profiles.values() for n in j['source_sha256'])|{'tools/matrixgame_actual_draw_checks.py','tools/matrixgame_actual_draw_audit.py','reports/matrixgame-e2e536-actual-draw.md','docs/matrixgame-readiness-plan.md','docs/matrixgame-init-sequence.md'}
 subprocess.run([git,'diff','--check','--',*sorted(names)],cwd=ROOT,check=True)
 for n in names:
  assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 hal=read(ROOT/'build/matrixgame/e2e534-reset-owners/windows/result.json');assert hal['status']=='FAIL' and 'cases=15 mismatches=2' in hal['stdout']
 result={'status':'E2E-5.36 PARTIAL / ACTUAL MAP NATIVE LINE BLOCKER REPRODUCED','profiles':profiles,
  'captures':captures,'total_draws':total,'unique_snapshots':unique,'rejected_line_snapshots':line_states,'rejected_line_draws':line_draws,
  'actual_callers':resolved,'Windows_HAL_original_game':'PASS 2 sessions; overall native state validation FAIL',
  'HOST_ASan_UBSan':'BASELINE REPRODUCED, expected guard exit 10; no sanitizer diagnostics','ARM64_QEMU':'BASELINE REPRODUCED, expected guard exit 10',
  'native_line_backend':'NOT IMPLEMENTED; production guard preserved','source_confirmed_frame_blocker':'CIFaceList::Render -> CIFaceElement::Render -> CMinimap::Draw -> DrawPrimitiveUP LINESTRIP, LASTPIXEL=1 -> DekoDevice::Record/NativeRenderState::ValidateDraw',
  'sdk_reachable_own_link_inputs':len(sdk['link_inputs_sha256']),'source_sha256':{n:sha(ROOT/n) for n in sorted(names)},
  'protected_verified':checked,'hardware_inputs_verified':len(hardware),'native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,
  'Switch_hardware':'NOT RUN / HARDWARE_PENDING','first_native_runtime_blocker':'NOT OBSERVED ON SWITCH; source-confirmed line blocker after preceding operations',
  'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN','E2E534_HAL_retry':'OPEN / FAIL'}
 result['initial_failure_artifacts']={str(p.relative_to(BASE)):sha(p) for p in (BASE/'initial-failure').iterdir() if p.is_file()}
 result['frame_sha256']={p.name:sha(p) for p in (BASE/'windows').glob('map-frame-*.png')}
 (ROOT/'reports/matrixgame-e2e536-actual-draw-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','total_draws','unique_snapshots','rejected_line_draws','sdk_reachable_own_link_inputs','protected_verified','hardware_inputs_verified')},ensure_ascii=False))
if __name__=='__main__':main()

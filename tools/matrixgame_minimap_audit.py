"""Strict original minimap/software/HAL evidence and protected-input audit."""
import json,re,subprocess,struct,zlib,hashlib
from pathlib import Path
from matrixgame_residency_audit import read,sha
from matrixgame_vector_object_overrides import function
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e528-minimap'
def png(path,gradient=False):
 data=path.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';w,h,depth,color=struct.unpack_from('>IIBB',data,16);assert (w,h,depth,color)==(512,512,8,2)
 offset=8;chunks=[]
 while offset<len(data):
  length=struct.unpack_from('>I',data,offset)[0];kind=data[offset+4:offset+8];chunk=data[offset+8:offset+8+length];assert zlib.crc32(kind+chunk)==struct.unpack_from('>I',data,offset+8+length)[0];offset+=12+length
  if kind==b'IDAT':chunks.append(chunk)
  if kind==b'IEND':break
 assert offset==len(data);raw=zlib.decompress(b''.join(chunks));assert len(raw)==h*(w*3+1)
 if gradient:
  previous=bytearray(w*3)
  for y in range(h):
   start=y*(w*3+1);filter=raw[start];row=bytearray(raw[start+1:start+1+w*3]);assert 0<=filter<=4
   for i in range(len(row)):
    a=row[i-3] if i>=3 else 0;b=previous[i];c=previous[i-3] if i>=3 else 0
    if filter==1:value=a
    elif filter==2:value=b
    elif filter==3:value=(a+b)//2
    elif filter==4:
     p=a+b-c;pa,pb,pc=abs(p-a),abs(p-b),abs(p-c);value=a if pa<=pb and pa<=pc else b if pb<=pc else c
    else:value=0
    row[i]=(row[i]+value)&255
   for x in range(w):assert tuple(row[x*3:x*3+3])==((x+y)%239,y%241,x%251),(x,y)
   previous=row
 return {'width':w,'height':h,'color_type':color,'sha256':sha(path),'gradient_independently_verified':gradient}
def main():
 results={}
 for kind in ('host','arm64','scopes-host','scopes-arm64','sdk','windows-frame'):
  j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
  for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
  results[kind]=j
 outputs={}
 for kind in ('host','arm64'):
  j=results[kind];assert 'CPU_GPU_DRIVER_MODEL_ONLY NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN' in j['stdout'];assert len(j['output_sha256'])==1
  for n,h in j['output_sha256'].items():assert sha(ROOT/n)==h;outputs[kind]=png(ROOT/n,True)
  link=next(c['command'] for c in j['commands'] if c['label']=='link');assert not any(s.endswith('.cpp') or s.endswith('libmatrixgame-native-cache.a') or '--allow-multiple-definition' in s for s in link)
  records=list((BASE/kind/'objects').glob('*.json'));assert len(records)==sum(s.endswith('.o') for s in link)
  for meta in records:
   item=read(meta);assert sha(meta.with_suffix('.o'))==item['object_sha256'];assert item['dependencies']
   for n,h in item['dependencies'].items():assert sha(Path(n))==h,n
 assert outputs['host']['sha256']==outputs['arm64']['sha256']
 assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
 for kind in ('scopes-host','scopes-arm64'):assert 'CPU_DRIVER_MODEL_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN' in results[kind]['stdout']
 assert '-fsanitize=address,undefined' in results['scopes-host']['commands'][0]['command']
 sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 ref=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];old=read(ROOT/'build/matrixgame/e2e527-output/source/native-engine-source.json')['files_sha256'];assert len(ref)==252
 changed=[n for n,h in ref.items() if old[n]!=h];assert changed==['MatrixGame/src/MatrixMinimap.cpp'],changed
 for directory in ('source','sdk/source'):
  assert read(BASE/directory/'native-engine-source.json')['files_sha256']==ref
  for n,h in ref.items():assert sha(BASE/directory/n)==h,n
 original=(ROOT/'build/matrixgame/e2e527-output/source/MatrixGame/src/MatrixMinimap.cpp').read_text();current=(BASE/'source/MatrixGame/src/MatrixMinimap.cpp').read_text();changed_methods=[]
 for signature in re.findall(r'(?m)^(?:void|bool|float|int)\s+(?:MATRIXGAME_NATIVE_CALLBACK\s+)?CMinimap::[\w:]+\(',original):
  if function(original,signature)!=function(current,signature):changed_methods.append(signature)
 expected={'StoreTexture','RestoreTexture','LoadMinimapFromCache','StoreMinimapToCache','RenderBackground','RenderObjectToBackground'}
 assert {s.split('CMinimap::',1)[1].split('(')[0] for s in changed_methods}==expected,changed_methods
 extraction=read(BASE/'fixture/extraction.json')
 for signature,h in extraction['method_sha256'].items():assert hashlib.sha256(function(current,signature).encode()).hexdigest()==h
 cache=(BASE/'source/portable-core/CacheNative.cpp').read_text()
 for signature,h in extraction['cache_method_sha256'].items():assert hashlib.sha256(function(cache,signature).encode()).hexdigest()==h
 for kind in ('host','arm64'):assert results[kind]['extraction']==extraction
 header=(BASE/'source/MatrixGame/src/MatrixMinimap.hpp').read_text();fixture=(BASE/'fixture/MinimapFixture.hpp').read_text();assert fixture==header.replace('class CMinimap : public CMain {','class CMinimap : public CMain {\n    friend struct NativeMinimapTestAccess;')
 windows=results['windows-frame'];text=windows['stdout'];assert text.count('original CGame::Init returned: ready=1')==2 and text.count('actual SDK draws=1008')==2
 for marker in ('MAPCACHE_COLD_RENDER','MAPCACHE_STORED','MAPCACHE_LOADED'):assert text.count(marker)==1,marker
 prior=read(ROOT/'build/matrixgame/e2e527-output/windows-frame/result.json');assert windows['snapshots']==prior['snapshots']
 for n,h in windows['snapshots'].items():assert sha(BASE/'windows-frame'/n)==h
 assert len(windows['output_files_sha256'])==1
 for n,h in windows['output_files_sha256'].items():assert sha(BASE/'windows-frame'/n)==h;outputs['windows-map-cache']=png(BASE/'windows-frame'/n)
 inventory=read(ROOT/'reports/matrixgame-e2e528-stored-state-inventory.json');assert inventory['render_count']==103 and inventory['native_render_setter_routes']==24 and len(inventory['missing_native_sampler_routes'])==5
 for n,h in inventory['source_sha256'].items():assert sha(ROOT/n)==h,n
 assert sha(Path(inventory['sdk_types_path']))==inventory['sdk_types_sha256']
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected=previous['protected'];checked=0
 for n,v in protected.items():
     if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
 for n,v in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
 package=ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg'
 assert sha(package)=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 init=read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')
 for n,h in init['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 original=read(ROOT/'reports/matrixgame-e2e521-native-entry-evidence.json')
 for n in ('tools/matrixgame_original_game_overrides.py',
     'tools/matrixgame_skin_manager_overrides.py','tools/matrixgame_render_pipeline_overrides.py'):
     assert sha(ROOT/n)==original['source_and_artifact_sha256_at_audit'][n],n

 parent=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip();assert parent==previous['parent']
 git=r'C:\Program Files\Git\cmd\git.exe';upstream=ROOT/'upstream/matrixgame'
 upstream_head=subprocess.check_output([git,'rev-parse','HEAD'],cwd=upstream,text=True).strip()
 upstream_status=subprocess.check_output([git,'status','--porcelain','--untracked-files=all'],cwd=upstream,text=True)
 assert upstream_head=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not upstream_status




 names=sorted(set(n for j in results.values() for n in j['source_sha256']))
 names+=['tools/matrixgame_minimap_audit.py','tools/matrixgame_stored_state_inventory.py','docs/matrixgame-readiness-plan.md','reports/matrixgame-e2e528-minimap.md']
 for n in names:
  s=(ROOT/n).read_text(encoding='utf-8');assert not any(re.match(r'^(<<<<<<<|=======|>>>>>>>)',line) for line in s.splitlines()),n
  if not n.startswith('build'):assert not any(line.rstrip()!=line for line in s.splitlines()),n
 p=subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
 assert not list(BASE.rglob('*.nro'))
 result={'stage':'E2E-5.28 MINIMAP LIFETIME/CACHE SOFTWARE PASS / FULL NATIVE INIT PARTIAL','parent':parent,'upstream_head':upstream_head,'results':results,'png':outputs,
 'source_sha256':{n:sha(ROOT/n) for n in names},'generated_changed':changed,'changed_methods':changed_methods,'extraction':extraction,'stored_state_inventory':inventory,
 'protected':protected,'protected_verified':checked,'hardware_inputs_verified':len(hardware),'primary_NRO':primary,'extra_frozen_artifacts_verified':previous['extra_frozen_artifacts_verified'],
 'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
 'known_remaining_exception':'map stored device state backend unavailable','known_remaining_caller':'CMatrixMap::ReleasePoolDefaultResources -> CDeviceState(map_device::StateBackend())',
 'first_failing_operation_of_full_native_Init':'NOT OBSERVED; no Switch execution permitted','NRO_created':False,'commit':False,'push':False,
 'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e528-minimap-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({'stage':result['stage'],'profiles':len(results),'protected':checked,'strong_undefined':len(sdk['strong_undefined']),'remaining':result['known_remaining_exception']}))
if __name__=='__main__':main()

"""Strict stencil/flat proof; independent GPU buffer audit, no HW runtime claim."""
import json,hashlib,re,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e531-stencil'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 results={}
 for kind in ('windows','host','arm64','sdk'):
  j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
  for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
  results[kind]=j
 text=results['windows']['stdout'];assert 'cases=2310 mismatches=0 CPU_model_mismatches=0' in text
 assert 'cases=72 mismatches=0 max_color_delta=0' in text and 'shadow_sequences=8 mismatches=0 max_color_delta=0' in text
 golden=BASE/'windows/stencil.golden';assert golden.stat().st_size==2310*72 and sha(golden)==results['windows']['golden_sha256']
 for kind in ('host','arm64'):
  j=results[kind];assert 'HAL_goldens=2310 exact original_scalar_restore=exact lease_restore=exact' in j['stdout']
  assert 'CPU_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN' in j['stdout'] and '-UNDEBUG' in j['commands'][0]['command']
  assert sha(BASE/kind/'stencil')==j['binary_sha256'] and sha(golden)==j['golden_sha256']
 assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
 for label,path,key in (('material',BASE/'materials/windows/result.json','input_sha256'),('fog',BASE/'fog/windows/result.json','source_sha256'),('alpha',BASE/'windows-alpha/result.json','source_sha256')):
  j=read(path);assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),label
  for n,h in j[key].items():assert sha(Path(n) if Path(n).is_absolute() else ROOT/n)==h,n
  results[label]=j
 assert 'D3D9_differential=1476 mismatches=0' in results['material']['stdout'] and 'GLSL_differential=1476 GLSL_mismatches=0' in results['material']['stdout']
 assert 'fixed vertex cases=84 GLSL_feedback=84 D3D9_ProcessVertices=80' in results['material']['stdout']
 assert 'cases=1344 gradients=168 sampled_pixels=193536 mismatches=0 max_delta=0' in results['fog']['stdout']
 assert '9184 cases PASS maximum delta=0' in results['alpha']['stdout']
 scene_file=BASE/'windows/shadow-scenes.bin';assert sha(scene_file)==results['windows']['shadow_scene_sha256'];data=scene_file.read_bytes();assert len(data)==8*2568;scenes={}
 for offset in range(0,len(data),2568):
  two,alpha=struct.unpack_from('<II',data,offset);hal=data[offset+8:offset+1032];gl=data[offset+1032:offset+2056];hs=data[offset+2056:offset+2312];gs=data[offset+2312:offset+2568]
  assert hal==gl and hs==gs and set(hs)=={0,1,255};assert (two,alpha) not in scenes;scenes[two,alpha]=(hal,hs)
 for alpha in (0,7,8,255):assert scenes[0,alpha]==scenes[1,alpha]
 for two in (0,1):assert scenes[two,0]==scenes[two,7] and scenes[two,8]==scenes[two,255] and scenes[two,0]!=scenes[two,8]
 sdk=results['sdk'];assert not sdk['strong_undefined'] and sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 caps=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared/d3d9caps.h');values={n:int(v,16) for n,v in re.findall(r'#define D3DSTENCILCAPS_(\w+)\s+0x([0-9A-Fa-f]+)',caps.read_text())}
 assert values=={'KEEP':1,'ZERO':2,'REPLACE':4,'INCRSAT':8,'DECRSAT':16,'INVERT':32,'INCR':64,'DECR':128,'TWOSIDED':256}
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(generated)==252 and generated==read(ROOT/'build/matrixgame/e2e530-fog/sdk/source/native-engine-source.json')['files_sha256']
 for n,h in generated.items():assert sha(BASE/'sdk/source'/n)==h,n
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
 parent=output('rev-parse','HEAD');assert parent==old['parent'];up=ROOT/'upstream/matrixgame';assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not output('status','--porcelain','--untracked-files=all',cwd=up)
 names=sorted(set(n for j in results.values() for n in j.get('source_sha256',{})))+['tools/matrixgame_stencil_audit.py','reports/matrixgame-e2e531-stencil.md','docs/matrixgame-readiness-plan.md']
 subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,check=True)
 for n in names:
  if not n.startswith('build'):assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.31 FLAT/STENCIL SOFTWARE PASS / FULL NATIVE INIT PARTIAL','results':results,'parent':parent,'source_sha256':{n:sha(ROOT/n) for n in names},
  'official_stencil_caps_sha256':sha(caps),'original_generated_files_unchanged':252,'protected_verified':checked,'hardware_inputs_verified':len(hardware),
  'shadow_buffers_independently_verified':8,'shadow_color_stencil_delta':0,'alpha_boundary_and_single_two_pass_equivalence':True,
  'edge_diagnostic_sha256':{str(p.relative_to(BASE)):sha(p) for p in (BASE/'edge-diagnostics').rglob('*') if p.is_file()},
  'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'complete_original_DrawShadows_executed_on_native':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
  'next_first_runtime_guard':'NOT OBSERVED; prior flat/StencilCaps guards closed in software','next_code_priority':'remaining actually-used native paths and original Takt/game loop; full map/frame/battle/main integration still open',
  'NRO_created':False,'commit':False,'push':False,'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e531-stencil-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','shadow_buffers_independently_verified','protected_verified','hardware_inputs_verified','next_first_runtime_guard')}))
if __name__=='__main__':main()
